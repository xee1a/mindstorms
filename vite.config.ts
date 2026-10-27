import { defineConfig, type Plugin } from 'vite';
import { readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';

const BLOCKLY_MEDIA = join(import.meta.dirname, 'node_modules', 'blockly', 'media');

/** Serves/emits Blockly's sprites under blockly-media/ so nothing loads from its CDN. */
function blocklyMedia(): Plugin {
  return {
    name: 'blockly-media',
    configureServer(server) {
      server.middlewares.use('/blockly-media', (req, res, next) => {
        const file = (req.url ?? '').split('?')[0].replace(/^\//, '');
        if (!/^[\w.-]+$/.test(file)) return next();
        try {
          const body = readFileSync(join(BLOCKLY_MEDIA, file));
          if (file.endsWith('.svg')) res.setHeader('Content-Type', 'image/svg+xml');
          res.end(body);
        } catch {
          next();
        }
      });
    },
    generateBundle() {
      for (const file of readdirSync(BLOCKLY_MEDIA)) {
        this.emitFile({ type: 'asset', fileName: `blockly-media/${file}`, source: readFileSync(join(BLOCKLY_MEDIA, file)) });
      }
    },
  };
}

export default defineConfig({
  // Relative asset paths so the built app loads from file:// inside Electron.
  base: './',
  plugins: [blocklyMedia()],
  server: { port: 5173, strictPort: true },
  // Loaded from disk by Electron, so one large Blockly chunk is fine.
  build: { outDir: 'dist', target: 'chrome120', chunkSizeWarningLimit: 1500 },
});
