#! /bin/sh
#
# Called once, at startup, before anything else.
#
export PATH="/sbin:/bin"
initset PATH="$PATH"
initset --on-ctrlaltdel reboot
initset --on-keyboard echo "I don't care!"

hostname --file /etc/hostname

#export LANG="el_GR.iso8859-7"
#initset LANG="$LANG"
#consolechars --tty=/dev/console -f iso07.f08 -m iso07 -u iso07 2>/dev/null
#loadkeys gr >/dev/null

if [ "$#" -eq 0 ] || [ "$#" -eq 1 -a "$1" = auto ]; then
	start console
	#start console tty syslog cron gpm acpi x
fi
