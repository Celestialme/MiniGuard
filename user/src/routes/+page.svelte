<script lang="ts">
	import { goto } from '$app/navigation';
	import ProgramPicker from '@src/components/ProgramPicker.svelte';
	import { invoke } from '@tauri-apps/api';
	let path: string | null = null;
	$: if (path) {
		invoke('run_exe', { path }).then((status) => {
			if (status == true) {
				goto('dashboard');
			}
		});
	}
</script>

<div class="body">
	<ProgramPicker bind:path />
</div>

<style>
	.body {
		width: 100%;
		height: 100%;
		display: flex;
		justify-content: center;
		align-items: center;
	}
</style>
