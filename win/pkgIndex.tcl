# Tcl package index file, version 1.0
#if {[info tclversion] < 8.0} return

proc mc_ifneeded dir {
    rename mc_ifneeded {}
    regsub {\.} [info tclversion] {} version
    package ifneeded Memchan @MEMCHAN_VERSION@ "package require Tk\nload [list [file join $dir @MEMCHAN_LIBFILE@$version.dll]] Memchan"
}

mc_ifneeded $dir
