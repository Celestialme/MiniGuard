export type Log = {
	id: number | null;
	original_file_name: string | null;
	current_file_name: string | null;
	original_operation: number | null;
	current_operation: number | null;
	backup_name: string | null;
	is_alive: boolean | null;
	count: number | null;
	date: string | null;
};
export type IgnorePath = {
	id: number;
	path: string;
};

export function asAny(obj: any): any {
	return obj;
}
export function asNumber(obj: any): number {
	return obj;
}
