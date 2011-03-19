#!/bin/sh
# A wrapper we use in the win32 build
#

outfile=""
while test $# -gt 0; do
	case "x$1" in
	x) shift; args="$args ''";;
	x-) shift; cat >_$$.c; args="$args _$$.c"; nukefiles=_$$.c;;
	x-o)
		shift
		case "x$1" in
		x*.o) args="$args /Fo$1";;
		xnetmud) outfile="/out:PennMUSH.exe";;
		x*) outfile="/out:$1.exe";;
		esac
		shift
		;;
	x-c) shift; args="$args /c";;
	x-E) shift; args="$args /E";;
	x-D*) args="$args `echo $1 | sed 's/^-/\//'`"; shift;;
	x-I*) args="$args `echo $1 | sed 's/^-/\//'`"; shift;;
	x-L*) libsearch="$libsearch `echo $1 | sed 's/^-L//'`"; shift;;
	x-l*)
		f="`echo $1 | sed 's/^-l//'`"
		for dir in . $libsearch; do
			test -f $dir/$f && args="$args $dir/$f"
		done
		shift;;
	x*) args="$args $1"; shift;;
	esac
done

case "x$outfile $args" in
x/out:*/link*) args="$args $outfile";;
x/out:*) args="$args /link $outfile";;
x*) args="$args $outfile";;
esac

echo "Translated to: cl /nologo $args"
cl /nologo $args
retcode=$?

case "x$nukefiles" in
 x) nukefiles="";;
 x*) rm $nukefiles;;
esac

exit $retcode

