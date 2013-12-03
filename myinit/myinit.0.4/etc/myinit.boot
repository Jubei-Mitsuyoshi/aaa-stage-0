#! /bin/sh
#
# Called once, at startup, before anything else.
#
export PATH="/sbin:/bin"
sbin/myinitset PATH="$PATH"
sbin/myinitset --on-ctrlaltdel sbin/myreboot
sbin/myinitset --on-keyboard echo "I don't care!"

hostname --file /etc/hostname

#export LANG="el_GR.iso8859-7"
#sbin/myinitset LANG="$LANG"
#consolechars --tty=/dev/console -f iso07.f08 -m iso07 -u iso07 2>/dev/null
#loadkeys gr >/dev/null

if [ "$#" -eq 0 ] || [ "$#" -eq 1 -a "$1" = auto ]; then
	sbin/mystart console
	#sbin/mystart console tty syslog cron gpm acpi x
fi
