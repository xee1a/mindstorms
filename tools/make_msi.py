"""Builds NXTStudio.msi from the dist folder, with Python's msilib (Python 3.12 or older; it was removed in 3.13).

    python tools/make_msi.py <dist folder> <output.msi> <version>

Installs for all users into Program Files\\NXT Studio, with Start menu and desktop shortcuts and an
uninstall entry. A newer (or the same) version replaces the installed one, so just run the new .msi.
"""
import os
import sys
import uuid
import warnings

warnings.filterwarnings("ignore", category=DeprecationWarning)
import msilib  # noqa: E402
from msilib import CAB, Directory, Feature, add_data, add_tables, schema, sequence  # noqa: E402

# Never change: Windows uses it to find older NXT Studio installs to replace.
UPGRADE_CODE = "{6F1C2B4E-8A3D-4E57-9C1B-2D7E5A0F3C91}"
APP_EXE = "NXTStudio.exe"


def build(dist, out, version):
    product_code = "{%s}" % str(uuid.uuid4()).upper()  # new for every build, as major upgrades need
    db = msilib.init_database(out, schema, "NXT Studio", product_code, version, "NXT Studio")
    add_tables(db, sequence)
    here = os.path.dirname(os.path.abspath(__file__))
    icon = os.path.join(here, "..", "resources", "app.ico")
    add_data(db, "Property", [
        ("UpgradeCode", UPGRADE_CODE),
        ("ALLUSERS", "1"),
        ("ARPPRODUCTICON", "app.ico"),
        ("ARPNOMODIFY", "1"),
        ("ARPNOREPAIR", "1"),
        ("SecureCustomProperties", "OLDPRODUCTS"),
    ])
    add_data(db, "Icon", [("app.ico", msilib.Binary(icon))])
    # Remove any installed version up to and including this one before installing.
    add_data(db, "Upgrade", [(UPGRADE_CODE, "0.0.0", version, None, 256 | 512, None, "OLDPRODUCTS")])

    cab = CAB("app")
    root = Directory(db, cab, None, dist, "TARGETDIR", "SourceDir")
    add_data(db, "Directory", [
        ("ProgramFiles64Folder", "TARGETDIR", "."),
        ("ProgramMenuFolder", "TARGETDIR", "."),
        ("DesktopFolder", "TARGETDIR", "."),
    ])
    feature = Feature(db, "App", "NXT Studio", "NXT Studio", 1, level=1, directory="INSTALLDIR")
    feature.set_current()
    app = Directory(db, cab, None, dist, "INSTALLDIR", "NXTSTU~1|NXT Studio")
    # Directory() above parents INSTALLDIR to nothing; point it at Program Files instead.
    db.OpenView("UPDATE Directory SET Directory_Parent='ProgramFiles64Folder' WHERE Directory='INSTALLDIR'").Execute(None)

    def add_folder(directory, path):
        files = sorted(f for f in os.listdir(path) if os.path.isfile(os.path.join(path, f)))
        subdirs = sorted(d for d in os.listdir(path) if os.path.isdir(os.path.join(path, d)))
        if files:
            key = APP_EXE if APP_EXE in files else files[0]
            directory.start_component(directory.logical, feature, 0, keyfile=key)
            for f in files:
                directory.add_file(f)
        for d in subdirs:
            sub = Directory(db, cab, directory, d, d, "%s|%s" % (directory.make_short(d), d))
            add_folder(sub, os.path.join(path, d))

    add_folder(app, dist)
    main_component = app.logical
    add_data(db, "Shortcut", [
        ("StartMenuShortcut", "ProgramMenuFolder", "NXTSTU~1|NXT Studio", main_component, "[INSTALLDIR]" + APP_EXE,
         None, "LEGO MINDSTORMS NXT", None, "app.ico", 0, None, "INSTALLDIR"),
        ("DesktopShortcut", "DesktopFolder", "NXTSTU~1|NXT Studio", main_component, "[INSTALLDIR]" + APP_EXE,
         None, "LEGO MINDSTORMS NXT", None, "app.ico", 0, None, "INSTALLDIR"),
    ])
    cab.commit(db)
    db.Commit()
    del root


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    build(os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2]), sys.argv[3])
    print("MSI:", sys.argv[2])
