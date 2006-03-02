# tmlib.sh

# Utilities useful for writing tmtest testfiles.
# This file is covered by the MIT License.

# DO NOT EDIT THIS FILE!  Edit /etc/tmtest.conf or ~/.tmtestrc instead.
# This file is replaced when you reinstall tmtest and your changes
# will be lost!


# TODO: should these routines be prefixed by "TM"?
# TODO: is there any way to get rid of MKFILE_EMPTY?  Can't MKFILE notice
#       if read would block and, if so, just create an empty file.?


# tmlib functions:
#
# ASSERT:   Stop a test if a condition fails
# TRAP:     Execute a command when an exception happens
# ATEXIT:   Ensure a command runs even if the test fails (usually to clean up).
# MKFILE_EMPTY: create an empty temporary file.
# MKFILE:   Creates a temporary file with the given contents.
# MKDIR:    create a temporary directory
# INDENT:   indent some output.
# REPLACE:  replaces literal text with other literal text (no regexes).



#
# ASSERT
#
# If you include this file from either a config file or your
# test file (". assert.sh" or "source assert.sh"),
# you can then use asserts in your test scripts.

# usage:
#   . assert.sh
# 	assert 42 -eq $((0x2A))		# true, so test continues as normal
# 	assert 1 -eq 2				# false, so the test is aborted.

ASSERT ()
{
	if [ ! $* ]; then
		msg=''
		if [ ${BASH_VERSINFO[0]} -ge 3 ]; then
			# bash2 doesn't provide BASH_SOURCE or BASH_LINENO
			msg=" on ${BASH_SOURCE[1]} line ${BASH_LINENO[0]}"
		fi
		ABORT assertion failed$msg: \"$*\"
	fi  
}    


#
# TRAP
#
# This function makes trap behave more like atexit(3), where you can
# install multiple commands to execute for the same condition.
# I'm surprised that Bash doesn't do this by default.
#
# NOTE: you must not mix use of TRAP and trap on the same conndition.
# The builtin trap will remove TRAP's condition, and all the commands
# installed using TRAP will not be run.
#
# Call this routine exactly like the trap builtin: "TRAP cmd cond"
#
# Example:
#
#     TRAP "echo debug!" DEBUG
#

TRAP ()
{
	# install the trap if this is the first time TRAP is called for
	# the given condition.  (Is there any way to get rid of "local var"??)

	local var=TRAP_$2
	if [ ! -n "${!var}" ]; then
		trap "eval \"\$TRAP_$2\"" $2
	fi


	# This just adds $1 to the front of the string given by TRAP_$2.
	# In Perl:		$TRAP{$2} = $1.($TRAP{$2} ? "; " : "").$TRAP{$2}

	eval TRAP_$2=\"$1'${'TRAP_$2':+; }$'TRAP_$2\"
}


#
# ATEXIT
#
# This behaves just like atexit(3) call.  Supply a command to be executed
# when the shell exits.  Commands are executed in the reverse order that
# they are supplied.
#
# Example:  (will produce "BA" on stdout when the test ends)
#
#     ATEXIT echo A
#     ATEXIT echo -n B

ATEXIT ()
{
	TRAP "$*" EXIT
}


#
# MKFILE
#
# Creates a file and assigns the new filename to the given variable.
# Fills in the new file with the supplied data.  Ensures that it is
# deleted when the test ends.
#
# argument 1: varname, the name of the variable that will contain the new filename.
# argument 2: filename, (optional) the name/fullpath to give the file.
#
# You need to be aware that if you supply an easily predictable filename
# (such as a PID), you are exposing your users to symlink attacks.  You
# should never supply a filename unless you know EXACTLY what you are doing.
#
# Examples:
#
# create a new file with a random name in $TMPDIR or /tmp:
#
#     MKFILE fn <<-EOL
#     	Initial file contents.
#     EOL
#     cat "$fn"		<-- prints "Initial file contents."
#
# create a new empty file with the given name (open to symlink attack,
# DO NOT USE UNLESS YOU ARE SURE WHAT YOU ARE DOING).
#
#     MKFILE ttf /tmp/$mydir/tt1 < /dev/null
#

MKFILE ()
{
	local name=${2-`mktemp -t tmtest.XXXXXX || ABORT MKFILE: could not mktemp`}
	eval "$1='$name'"
	cat > "$name"
	ATEXIT "rm '$name'"
}


#
# MKFILE_EMPTY
#
# I can't figure out how to get bash to bail instead of blocking.
# Therefore, if you just want to create an empty file, you either
# call MKFILE piped from /dev/null or just call MKFILE_EMPTY.
#

MKFILE_EMPTY ()
{
	MKFILE "$*" < /dev/null
}


#
# MKDIR
#
# Like MKFILE, but creates a directory instead of a file.  If you
# supply a directory name, and that directory already exists, then
# MKDIR ensures it is deleted when the script ends.  The directory
# will not be deleted if it still contains any files.
#
# argument 1: varname, the name of the variable that will contain the new directory name.
# argument 2: dirname, (optional) the name/fullpath to give the directory.
#
# Examples:
#
# create a new directory with a random name in $TMPDIR or /tmp:
#
#     MKDIR dn
#     cd "$dn"
#

MKDIR ()
{
	local name=${2}
	if [ -z "$name" ]; then
		name=`mktemp -d -t tmtest.XXXXXX || ABORT MKDIR: could not mktemp`
	else
		[ -d $name ] || mkdir --mode 0600 $name || ABORT "MKDIR: could not 'mkdir \"$name\"'"
	fi

	eval "$1='$name'"
	ATEXIT "rmdir '$name'"
}


# 
# INDENT
#
# Indents the output the given number of spaces.
# Note that this only works with stdout!  You'll have to combine
# the stdout and stderr streams if you want to indent stderr.
#
# By default this script indents each line with four spaces.
# Pass an argument to tell this function what to put before
# each line.
#
# Examples:
#
#    echo hi | INDENT "\t"      # indents stdout with a tab char
#    cat /tmp 2>&1 | INDENT     # indents both stdout and stderr
#    exec > >(INDENT)           # indents all further script output
#

INDENT ()
{
    # sed appears more binary transparent than bash's builtins so I'm
    # using sed instead of builtin read.  It might even be faster.

    sed -e "s/^/${1-    }/"

    # even though this might be faster, it mucks things up.
    #
    #	while read LINE; do
    #		echo $'\t'"$LINE"
    #	done
}


# 
# REPLACE
#
# Replaces all occurrences of the first argument with the second
# argument.  Both should be regex-safe.  Use sed if you want to
# replace with regexes.  NOTE: replace does not work if a newline
# is embedded in either argument.
#
# Three layers of escaping!  (bash, perlvar, perlre)  This is insane.
# I wish sed or awk would work with raw strings instead of regexes.
# Why isn't a replace utility a part of Gnu coreutils?
#

REPLACE()
{
    # unfortunately bash can't handle this substitution because it
    # must work on ' and \ simultaneously.  So, send it to perl for
    # processing.  Until ' and \ have been escaped, Perl can't 
    

#    echo "got: '$1' '${1//[\'\\]/\'}'"
#    perl -e "print \"in: \" . quotemeta('${1//\'/\'}') . \"\\n\";"

#     (echo "$1"; echo "$2"; cat) | cat
#     (echo "$1"; echo "$2"; cat) | perl -e "my \$in = <>; chomp \$in; my \$out = <>; chomp \$out; print '  in: <<' . \$in . \">>\n out: <<\" . \$out . \">>\n\"; while(<>) { print \"DATA: \$_\" }"
#     (echo "$1"; echo "$2"; cat) | perl -e "my \$in = <>; chomp \$in; \$in=quotemeta(\$in); my \$out = <>; chomp \$out; print '  in: <<' . \$in . \">>\n out: <<\" . \$out . \">>\n\"; while(<>) { print \"DATA: \$_\" }"

     (echo "$1"; echo "$2"; cat) | perl -e "my \$in = <>; chomp \$in; \$in=quotemeta(\$in); my \$out = <>; chomp \$out; while(<>) { s/\$in/\$out/g; print or die \"Could not print: \$!\\\\n\"; }"


# this scheme also works but it's much better to feed the vars on stdin
# along with the data.  Less process overhead, simpler script.  This
# does mean that REPLACE won't work with embedded newlines though.
#
#    local in=$(perl -p -e "s/([\\'\\\\])/\\\\\$1/g" <<< $1);
#    local out=$(perl -p -e "s/([\\'\\\\])/\\\\\$1/g" <<< $2);
#
#     perl -e "<>; my \$in = quotemeta(chomp); <>; my \$out = chomp; while(<>) { s/\$in/\$out/g; print or die \"Could not print: \$!\\\\n\"; }"
#    perl -e "my \$in = quotemeta('${1//\'/\'}'); my \$out = '${2//\'/\'}'; while(<>) { s/\$in/\$out/g; print or die \"Could not print: \$!\\\\n\"; }"
}
