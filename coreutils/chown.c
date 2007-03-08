/* vi: set sw=4 ts=4: */
/*
 * Mini chown implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 defects - unsupported options -H, -L, and -P. */
/* BB_AUDIT GNU defects - unsupported long options. */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/chown.html */

#include "busybox.h"

static struct bb_uidgid_t ugid = { -1, -1 };

static int (*chown_func)(const char *, uid_t, gid_t) = chown;

#define OPT_STR     ("Rh" USE_DESKTOP("vcfLHP"))
#define BIT_RECURSE 1
#define BIT_NODEREF 2
#define BIT_TRAVERSE 0x20
#define BIT_TRAVERSETOP (0x20|0x40)
#define OPT_RECURSE (option_mask32 & 1)
#define OPT_NODEREF (option_mask32 & 2)
#define OPT_VERBOSE (USE_DESKTOP(option_mask32 & 0x04) SKIP_DESKTOP(0))
#define OPT_CHANGED (USE_DESKTOP(option_mask32 & 0x08) SKIP_DESKTOP(0))
#define OPT_QUIET   (USE_DESKTOP(option_mask32 & 0x10) SKIP_DESKTOP(0))
/* POSIX options
 * -L traverse every symbolic link to a directory encountered
 * -H if a command line argument is a symbolic link to a directory, traverse it
 * -P do not traverse any symbolic links (default)
 * We do not conform to the following:
 * "Specifying more than one of -H, -L, and -P is not an error.
 * The last option specified shall determine the behavior of the utility." */
/* -L */
#define OPT_TRAVERSE    (USE_DESKTOP(option_mask32 & BIT_TRAVERSE) SKIP_DESKTOP(0))
/* -H or -L */
#define OPT_TRAVERSETOP (USE_DESKTOP(option_mask32 & BIT_TRAVERSETOP) SKIP_DESKTOP(0))

static int fileAction(const char *fileName, struct stat *statbuf,
		void ATTRIBUTE_UNUSED *junk, int depth)
{
	if (!chown_func(fileName,
			(ugid.uid == (uid_t)-1) ? statbuf->st_uid : ugid.uid,
			(ugid.gid == (gid_t)-1) ? statbuf->st_gid : ugid.gid)
	) {
		if (OPT_VERBOSE
		 || (OPT_CHANGED && (statbuf->st_uid != ugid.uid || statbuf->st_gid != ugid.gid))
		) {
			printf("changed ownership of '%s' to %u:%u\n",
					fileName, ugid.uid, ugid.gid);
		}
		return TRUE;
	}
	if (!OPT_QUIET)
		bb_perror_msg("%s", fileName);	/* A filename can have % in it... */
	return FALSE;
}

int chown_main(int argc, char **argv);
int chown_main(int argc, char **argv)
{
	int retval = EXIT_SUCCESS;

	opt_complementary = "-2";
	getopt32(argc, argv, OPT_STR);
	argv += optind;

	/* This matches coreutils behavior (almost - see below) */
	if (OPT_NODEREF
	    /* || (OPT_RECURSE && !OPT_TRAVERSETOP): */
	    USE_DESKTOP( || (option_mask32 & (BIT_RECURSE|BIT_TRAVERSETOP)) == BIT_RECURSE)
	) {
		chown_func = lchown;
	}

	parse_chown_usergroup_or_die(&ugid, argv[0]);

	/* Ok, ready to do the deed now */
	argv++;
	do {
		char *arg = *argv;

		if (OPT_TRAVERSETOP) {
			/* resolves symlink (even recursive) */
			arg = xmalloc_realpath(arg);
			if (!arg)
				continue;
		}

		if (!recursive_action(arg,
				OPT_RECURSE,    // recurse
				OPT_TRAVERSE,   // follow links if -L
				FALSE,          // depth first
				fileAction,     // file action
				fileAction,     // dir action
				NULL,           // user data
				0)              // depth
		) {
			retval = EXIT_FAILURE;
		}

		if (OPT_TRAVERSETOP)
			free(arg);
	} while (*++argv);

	return retval;
}

/*
Testcase. Run in empty directory.

#!/bin/sh
t1="/tmp/busybox chown"
t2="/usr/bin/chown"
create() {
    rm -rf $1; mkdir $1
    (
    cd $1 || exit 1
    mkdir dir dir2
    >up
    >file
    >dir/file
    >dir2/file
    ln -s dir linkdir
    ln -s file linkfile
    ln -s ../up dir/linkup
    ln -s ../dir2 dir/linkupdir2
    )
    chown -R 0:0 $1
}
tst() {
    create test1
    create test2
    (cd test1; $t1 $1)
    (cd test2; $t2 $1)
    (cd test1; ls -lnR) >out1
    (cd test2; ls -lnR) >out2
    echo "chown $1" >out.diff
    if ! diff -u out1 out2 >>out.diff; then exit 1; fi
    rm out.diff
}
tst_for_each() {
    tst "$1 1:1 file"
    tst "$1 1:1 dir"
    tst "$1 1:1 linkdir"
    tst "$1 1:1 linkfile"
}
echo "If script produced 'out.diff' file, then at least one testcase failed"
# These match coreutils 6.8:
tst_for_each ""
tst_for_each "-R"
tst_for_each "-RP"
tst_for_each "-RL"
tst_for_each "-RH"
tst_for_each "-h"
tst_for_each "-hR"
tst_for_each "-hRP"
# Below: with "chown linkdir" coreutils 6.8 will chown linkdir _target_,
# we lchown _the link_. I believe we are "more correct".
#tst_for_each "-hRL"
#tst_for_each "-hRH"

*/
