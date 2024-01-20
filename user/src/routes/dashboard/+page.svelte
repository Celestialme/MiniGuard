<script lang="ts">
	import { listen } from '@tauri-apps/api/event';
	import { invoke } from '@tauri-apps/api/tauri';
	import { type Log, asAny, asNumber, type IgnorePath } from '@src/types';
	import { open } from '@tauri-apps/api/dialog';
	let logs: Array<Log> = [];
	let undo_dialog = false;
	let undo_id = 0;
	let show_ignore_paths = false;
	let ignore_paths: Array<IgnorePath> = [];
	invoke('get_logs').then((res) => (logs = res as Array<Log>));
	const unlisten = listen('updated_logs', (event) => {
		logs = event.payload as Array<Log>;
	});

	function open_undo_dialog(id: number | null) {
		undo_dialog = true;
		undo_id = id as number;
	}
	let operations: { [key: number]: string } = {
		1: 'CREATE',
		2: 'WRITE',
		3: 'RENAME',
		4: 'DELETE'
	} as const;

	async function select_path() {
		let path = (await open({
			multiple: false,
			directory: true
		})) as string | null;
		if (path) {
			ignore_paths = [...ignore_paths, { id: 0, path }];
			invoke('add_ignore_path', { path });
		}
	}

	function showIgnorePaths() {
		show_ignore_paths = !show_ignore_paths;
		if (show_ignore_paths) {
			invoke('get_ignore_paths').then((paths) => {
				ignore_paths = paths as Array<IgnorePath>;
			});
		}
	}
</script>

<div class="body">
	<div class="top_panel">
		<div class="search">
			<input type="text" />
			<button>search</button>
		</div>
		<button class="ignore_paths" on:click={showIgnorePaths}>IGNORE PATHS</button>
	</div>

	<table>
		<thead>
			<tr>
				<th>ID</th>
				<th>Original fileName</th>
				<th>Current fileName</th>
				<th>Original operation</th>
				<th>Current operation</th>
				<th>Backup path</th>
				<th>Alive</th>
				<th>Count</th>
				<th>DATE</th>
			</tr>
		</thead>
		<tbody>
			{#each logs as log}
				<tr on:click={() => open_undo_dialog(log.id)}>
					<td>{log.id}</td>
					<td data-path={log.original_file_name} class="name"
						><span>{log.original_file_name?.split('\\').slice(-1)[0]}<span /></span></td
					>
					<td data-path={log.current_file_name} class="name"
						><span>{log.current_file_name?.split('\\').slice(-1)[0]}<span /></span></td
					>
					<td>{operations[asNumber(log.original_operation)]}</td>
					<td>{operations[asNumber(log.current_operation)]}</td>
					<td data-path={log.backup_name} class="name"><span>{log.backup_name}<span /></span></td>
					<td>{log.is_alive}</td>
					<td>{log.count}</td>
					<td>{log.date}</td>
				</tr>
			{/each}
		</tbody>
	</table>
</div>
{#if undo_dialog}
	<div class="backdrop"></div>
	<div class="undo_dialog">
		Do you want to undo changes?

		<div>
			<button
				on:click={() => {
					undo_dialog = false;
					invoke('undo', { id: undo_id });
				}}>Yes</button
			>
			<button on:click={() => (undo_dialog = false)}>NO</button>
		</div>
	</div>
{/if}

{#if show_ignore_paths}
	<div class="backdrop" on:click={() => (show_ignore_paths = false)} />
	<div class="ignore_paths_wrapper">
		<p>IGNORE PATHS</p>
		<button class="add_path" on:click={select_path}>ADD PATH</button>
		{#each ignore_paths as path, index}
			<div class="ignore_path_row">
				<input disabled value={path.path} />
				<button
					class="delete_path"
					on:click={() => {
						ignore_paths.splice(index, 1);
						ignore_paths = ignore_paths;
						invoke('remove_ignore_path', { pathId: path.id });
					}}>DELETE</button
				>
			</div>
		{/each}
	</div>
{/if}

<style>
	.body {
		width: 100%;
		height: 100%;
		display: flex;
		flex-direction: column;
		justify-content: flex-start;
		align-items: center;
		gap: 10px;
	}
	table {
		font-family: arial, sans-serif;
		border-collapse: collapse;
		width: 100%;
		font-family: 'Poppins';
	}

	td,
	th {
		border: 1px solid #dddddd;
		text-align: left;
		padding: 8px;
	}
	tbody tr {
		cursor: pointer;
	}
	tbody tr:hover {
		background-color: rgb(81, 245, 245);
	}
	.name span {
		display: block;
		max-width: 200px;
		overflow: hidden;
		text-overflow: ellipsis;
		white-space: nowrap;
	}
	.name {
		position: relative;
	}
	.name:hover::after {
		content: attr(data-path);
		display: block;
		position: absolute;
		left: 0;
		bottom: -100%;
		background-color: black;
		padding: 20px 50px;
		color: white;
		z-index: 1;
		pointer-events: none;
		border-radius: 12px;
	}
	tr:nth-child(even) {
		background-color: #dddddd;
	}
	.backdrop {
		position: fixed;
		top: 0;
		left: 0;
		width: 100%;
		height: 100%;
		background-color: rgba(0, 0, 0, 0.5);
	}
	.undo_dialog {
		position: fixed;
		top: 50%;
		left: 50%;
		transform: translate(-50%, -50%);
		padding: 50px;
		background-color: white;
		display: flex;
		flex-direction: column;
		justify-content: center;
		align-items: center;
		gap: 20px;
		border: 2px solid black;
		border-radius: 12px;
		font-family: 'Poppins';
	}
	.undo_dialog button {
		padding: 10px 40px;
		border-radius: 12px;
		cursor: pointer;
		border: 1px solid black;
		font-family: 'Poppins';
	}
	.undo_dialog button:active {
		transform: scale(0.9);
	}
	.top_panel {
		width: 100%;
		display: flex;
		align-items: center;
	}
	.search input {
		font-size: 25px;
		height: 100%;
		border: 1px solid gray;
		border-top-left-radius: 12px;
		border-bottom-left-radius: 12px;
		outline: none;
		padding: 5px;
	}
	.search button {
		padding: 10px 20px;
		height: 100%;
	}
	.search {
		height: 40px;
		display: flex;
		justify-content: center;
		align-items: center;
		margin: auto;
		padding-left: 100px;
	}
	.ignore_paths,
	.add_path,
	.delete_path {
		border: 1px solid gray;
		font-family: 'Poppins';
		border-radius: 12px;
		font-size: 15px;
		padding: 5px 10px;
	}
	.delete_path {
		border-radius: 0;
		height: 100%;
	}
	button:active {
		transform: scale(0.9);
	}
	.ignore_paths_wrapper {
		position: fixed;
		width: 80%;
		border-radius: 12px;
		display: flex;
		flex-direction: column;
		align-items: center;
		top: 50%;
		left: 50%;
		transform: translate(-50%, -50%);
		background-color: white;
		font-family: 'Poppins';
		gap: 10px;
		padding: 0 10px;
		padding-bottom: 20px;
	}
	.ignore_path_row {
		display: flex;
		justify-content: space-around;
		align-items: center;
		width: 100%;
		height: 50px;
	}

	.ignore_path_row input {
		padding: 10px;
		flex-grow: 1;
		font-family: 'Poppins';
		font-size: 15px;
		border: 1px solid gray;
		height: 100%;
	}
</style>
