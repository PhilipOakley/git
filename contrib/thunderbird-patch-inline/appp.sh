#!/bin/sh
# Copyright 2008 Lukas Sandström <luksan@gmail.com>
#
# AppendPatch - A script to be used together with ExternalEditor
# for Mozilla Thunderbird to properly include patches inline in e-mails.

# ExternalEditor can be downloaded at http://globs.org/articles.php?lng=en&pg=2

# NOTE: You must change some words in this script according to the language
# used by Mozilla Thunderbird, as <Subject>, <To>, <Don't remove this line>.

CONFFILE=~/.appprc

# Change "Dont' remove this line" to what Thunderbird writs for your language
SEP="-=-=-=-=-=-=-=-=-=# Don't remove this line #=-=-=-=-=-=-=-=-=-"
if [ -e "$CONFFILE" ] ; then
	LAST_DIR=`grep -m 1 "^LAST_DIR=" "${CONFFILE}"|sed -e 's/^LAST_DIR=//'`
	cd "${LAST_DIR}"
else
	cd > /dev/null
fi

PATCH=$(zenity --file-selection)

if [ "$?" != "0" ] ; then
	#zenity --error --text "No patchfile given."
	exit 1
fi

cd - > /dev/null

SUBJECT=`sed -n -e '/^Subject: /p' "${PATCH}"`
BODY=`sed -e "1,/${SEP}/d" $1`
CMT_MSG=`sed -e '1,/^$/d' -e '/^---$/,$d' "${PATCH}"`
DIFF=`sed -e '1,/^---$/d' "${PATCH}"`
PATCHTMP="${PATCH}.tmp"
sed '/^$/q' "${PATCH}" >"$PATCHTMP"

export PATCHTMP
CCS=`perl -e 'local $/=undef; open FILE, $ENV{'PATCHTMP'}; $text=<FILE>;
close FILE; $addr = $1 if $text =~ /Cc: (.*?(,\n .*?)*)\n/s; $addr =~ s/\n//g;
print $addr;'`

TO=`perl -e 'local $/=undef; open FILE, $ENV{'PATCHTMP'}; $text=<FILE>;
close FILE; $addr = $1 if $text =~ /To: (.*?(,\n .*?)*)\n/s; $addr =~ s/\n//g;
print $addr;'`

rm -rf $PATCHTMP

# Change Subject: before next line according to Thunderbird language
# for example:
# SUBJECT=`echo $SUBJECT | sed -e 's/Subject:/Oggetto:/'`
echo "$SUBJECT" > $1
# Change To: according to Thunderbird language
echo "To: $TO" >> $1
echo "Cc: $CCS" >> $1
echo "$SEP" >> $1

echo "$CMT_MSG" >> $1
echo "---" >> $1
if [ "x${BODY}x" != "xx" ] ; then
	echo >> $1
	echo "$BODY" >> $1
	echo >> $1
fi
echo "$DIFF" >> $1

LAST_DIR=`dirname "${PATCH}"`

grep -v "^LAST_DIR=" "${CONFFILE}" > "${CONFFILE}_"
echo "LAST_DIR=${LAST_DIR}" >> "${CONFFILE}_"
mv "${CONFFILE}_" "${CONFFILE}"
