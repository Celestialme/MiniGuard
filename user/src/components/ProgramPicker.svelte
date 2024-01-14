<script lang="ts">
	import SvgIcon from './SvgIcon.svelte';
	import { open } from '@tauri-apps/api/dialog';

	export let path: string | null;

	async function fileOpen() {
		path = (await open({
			multiple: false,
			filters: [
				{
					name: 'Program',
					extensions: ['exe']
				}
			]
		})) as string | null;
	}
</script>

<div class="container" on:pointerdown={() => fileOpen()}>
	<SvgIcon />
	<p>click to Select EXE</p>
</div>

<style>
	.container {
		display: flex;
		justify-content: center;
		align-items: center;
		width: calc(100% - 50px);
		height: 80%;
		max-height: 760px;
		margin: 0 20px;
		user-select: none;
		background-color: #dbdbdb;
		border-radius: 12px;
		flex-direction: column;
		box-shadow: 7px 4px 17px 0px #ffffff8c;
		font-family: 'Poppins';
	}
</style>
