#!/bin/sh
# Compare the compositor's advertised globals against globals.expected.
#
#   check-globals.sh <run-nested.sh> <e_test_globals> <globals.expected>
#
# A failure here is either a regression (a global disappeared or lost a
# version) or an intentional change that has not been recorded yet. For the
# latter, update globals.expected in the same commit as the protocol change --
# that file is the reviewable record of E's Wayland protocol surface.
#
# Each line of globals.expected is "<interface><TAB><version>", and the version
# may be written ">=N" instead of an exact number. That is for the globals E
# does not create and whose version it therefore does not choose: pinning those
# exactly records a property of a dependency rather than of E, and fails on a
# machine with a different one for a reason that has nothing to do with this
# tree. Use it sparingly -- an exact version is the whole point of the file for
# everything E does decide, and ">=" on one of those would hide a real
# regression.

set -eu

if [ $# -ne 3 ]; then
    echo "usage: $0 <run-nested.sh> <e_test_globals> <globals.expected>" >&2
    exit 2
fi

RUNNER=$1
DUMPER=$2
EXPECTED=$3

ACTUAL=$(mktemp "${TMPDIR:-/tmp}/e-globals.XXXXXX")
trap 'rm -f "$ACTUAL"' EXIT INT TERM

"$RUNNER" "$DUMPER" >"$ACTUAL"

# The report keeps diff's '-' and '+' idiom, because that is what the file's
# readers already know, and adds '!' for a version that is present but wrong.
REPORT=$(awk '
FILENAME == ARGV[1] {
    want[$1] = $2
    order[++n] = $1
    next
}
{
    got[$1] = $2
}
END {
    for (i = 1; i <= n; i++)
      {
         name = order[i]
         if (!(name in got))
           {
              print "-" name "\t" want[name] "  (not advertised)"
              bad = 1
              continue
           }
         if (want[name] ~ /^>=/)
           {
              floor = substr(want[name], 3) + 0
              if (got[name] + 0 < floor)
                {
                   print "!" name "\t" got[name] "  (expected " want[name] ")"
                   bad = 1
                }
           }
         else if (got[name] != want[name])
           {
              print "!" name "\t" got[name] "  (expected " want[name] ")"
              bad = 1
           }
      }
    for (name in got)
      if (!(name in want))
        {
           print "+" name "\t" got[name]
           bad = 1
        }
    exit bad ? 1 : 0
}
' "$EXPECTED" "$ACTUAL") && exit 0

printf '%s\n' "$REPORT" >&2

cat >&2 <<EOF

check-globals: advertised globals do not match $EXPECTED

  '-' expected but not advertised -> a regression, or a global that moved
  '!' advertised at another version -> a version bump, intended or not
  '+' advertised but not expected -> update globals.expected in the same
                                     commit that added them

To refresh the file after an intentional change:
  $RUNNER $DUMPER > $EXPECTED

but read the result before committing it: a blind refresh turns every ">="
line back into an exact version and quietly re-pins something this tree does
not control.
EOF
exit 1
