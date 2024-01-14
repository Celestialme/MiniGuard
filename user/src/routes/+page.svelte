<script lang="ts">
	import { goto } from '$app/navigation';
	import ProgramPicker from '@src/components/ProgramPicker.svelte';
	let path: string | null = null;
	$: if (path) {
		goto('dashboard');
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
