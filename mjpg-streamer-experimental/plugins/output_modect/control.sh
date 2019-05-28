#!/bin/sh

# dynamically get or set output_modect.so parameters

which netcat >/dev/null 2>&1
if [ $? -ne 0 ]; then
	echo You need to install netcat to use this script
	exit 1
fi

usage() {
	echo "usage: $0 [-h host] [-p port] [-i pnum| -o pnum] [[-j] | [name] [value]]"
	echo "If value given, sets the named parameter to value,"
	echo	 "else returns the current value"
	echo "options:"
	echo "-h host: set the host name or ip default: localhost"
	echo "-p port: set the port number default: 8080"
	echo "-o plugin: set the output plugin number default: 0"
	echo "-j: just output the json decriptor file and exit"
	echo  "-i plugin: set the input plugin number"
	echo "Examples:"
	echo  "View current motion status"
	echo "	$0 -p $your_output_http_port -o $plugin_no  motion"
	echo "Turn on informational output"
	echo "	$0 -p $your_output_http_port -o $plugin_no debug 1"
	echo "view input_uvc dynamic controls"
	echo "	$0 -p $your_output_http_port -i 0 -j"
	exit 1
}

out() {
	echo "$1"
	exit $2
}

getval() {
	echo "$txt"|grep "$1:"|awk '{print $2}'
}

#defaults
host=localhost
port=8091
plugin_num=0
plugin=output_
json=0

while getopts ":h:p:i:o:j" arg; do
	case "${arg}" in
		h)
			host=${OPTARG}
			;;
		p)
			port=${OPTARG}
			;;
		i)
			plugin_num=${OPTARG}
			plugin=input_
			;;
		o)
			plugin_num=${OPTARG}
			plugin=output_
			;;
		j)
			json=1
			;;
		*)
			usage
			;;
	esac
done
shift $((OPTIND-1))

#get the descriptor json file 
txt=`printf "GET /$plugin$plugin_num.json\n\n"|nc $host $port 2>&1`

# check netcat status
[ "$?" -ne 0 ] && out "$txt" 1

#check mjpg-streamer response
echo "$txt"|grep -q "HTTP/1.0[[:space:]]200[[:space:]]OK" || out  "`echo "$txt"|tail -n 2`" 1

#json requested: prettify & exit
[ $json -eq 1 ] && out "`echo "$txt"|sed -e '1,11d' -e 's/[,\"{}]//g' -e 's/\]//'|sed  '/^$/N;/^\n$/D'`" 0

#get the text corresponding to named variable
txt=`echo "$txt"|sed -e 's/controls//' -e 's/[,\"]//g'|grep -A10 "$1"`
lines=`echo "$txt"|wc -l`

[ $lines -lt 11 ] && out "$1: not found" 1

#if more than one match found, try to resolve by matching the whole word
if [ $lines -gt 11 ]; then
	txt2=`echo "$txt"|grep -w -A10 "$1"`
	lines=`echo "$txt2"|wc -l`
	[ $lines -ne 11 ] &&	out "$1: ambiguous: `echo $(getval name)|sed 's/\n/ /g'`" 1
	txt="$txt2"
fi

#  no value supplied? we're done
[ -z "$2" ] && out `getval value` 0

name=`getval name`

[ "$2" -eq "$2" ] 2>/dev/null || out "$name: $2: not an integer" 1

max=`getval max`
min=`getval min`
[ "$2" -lt $min -o "$2" -gt $max ] && out "$name: $2 out of range (min $min max $max)" 1

id=`getval id`
g=`getval group`
d=`getval dest`
response=`printf "GET /?action=command&id=$id&dest=$d&group=$g&value=$2&plugin=$plugin_num\n\n"|nc $host $port 2>&1`

# check netcat status
[ "$?" -ne 0 ] && out  "$response" 1

# check mjpg-streamer response
echo "$response"|grep -q "HTTP/1.0[[:space:]]200[[:space:]]OK" || out  "`echo "$response"|tail -n 2`" 1

exit 0

