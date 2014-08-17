//valac --pkg gtk+-3.0 caller.vala

using GLib.Process;

int main (string[] args) {
    Gtk.init (ref args);

	var window = new Gtk.Window ();

	window.title = "System Status";
	window.set_border_width (26);
	window.set_position (Gtk.WindowPosition.CENTER);
	window.set_default_size (1900, 1200);
	window.destroy.connect (Gtk.main_quit);


	var button_iptables = new Gtk.Button.with_label ("IPTables");
	button_iptables.clicked.connect (() => {
	string i = call("iptables");
    button_iptables.label = i.to_string();
	});

	window.add (button_iptables);

	window.show_all ();

	Gtk.main ();
	return 0;
}

string call(string arg){

	string[] spawn_args = {"syslogparse", arg};
	string[] spawn_env = Environ.get ();
	string ls_stdout;
	int ls_status;

	Process.spawn_sync (
						"/",
						spawn_args,
						spawn_env,
						SpawnFlags.SEARCH_PATH,
						null,
						out ls_stdout,
						null,
						out ls_status
						);

	stdout.printf (ls_stdout);
	stdout.printf ("status:\n%d", ls_status);


return arg;
}