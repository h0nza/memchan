# -*- tcl -*-
# Manually created package index for Memchan @mFullVersion@

if {[string compare $::tcl_platform(platform) windows] == 0} {
    set ___libn [file join $dir .. .. bin Memchan@mShortDosVersion@]
} else {
    set ___libn [file join $dir libMemchan@mVersion@]
}

package ifneeded Memchan @mVersion@ [list load $___libn[info sharedlibextension]]
unset ___libn
