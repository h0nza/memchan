# -*- tcl -*-
# Manually created package index for Memchan @mFullVersion@

if {[string compare $::tcl_platform(platform) windows] == 0} {
    set ___libn [file join $dir .. .. bin Memchan22]
} else {
    set ___libn [file join $dir libMemchan2.2]
}

package ifneeded Memchan 2.2 [list load $___libn[info sharedlibextension]]
unset ___libn
