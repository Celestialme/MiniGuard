import { sveltekit } from '@sveltejs/kit/vite';
import { defineConfig } from 'vite';
import Path from 'path';
export default defineConfig({
	plugins: [sveltekit()],
	resolve: {
		alias: {
			'@src': Path.resolve('src/'),
			'@static': Path.resolve('static/'),
			'@root': Path.resolve('./')
		}
	}
});
