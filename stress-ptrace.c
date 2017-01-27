/*
 * Copyright (C) 2013-2017 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * This code is a complete clean re-write of the stress tool by
 * Colin Ian King <colin.king@canonical.com> and attempts to be
 * backwardly compatible with the stress tool by Amos Waterland
 * <apw@rossby.metr.ou.edu> but has more stress tests and more
 * functionality.
 *
 */
#include "stress-ng.h"
#if defined(__linux__)

#include <sys/ptrace.h>

/*
 *  main syscall ptrace loop
 */
static inline bool stress_syscall_wait(
	args_t *args,
	const pid_t pid)
{
	while (opt_do_run) {
		int status;

		if (ptrace(PTRACE_SYSCALL, pid, 0, 0) < 0) {
			pr_fail_dbg("ptrace");
			return true;
		}
		if (waitpid(pid, &status, 0) < 0) {
			if (errno != EINTR)
				pr_fail_dbg("waitpid");
			return true;
		}

		if (WIFSTOPPED(status) &&
		    (WSTOPSIG(status) & 0x80))
			return false;
		if (WIFEXITED(status))
			return true;
	}
	return true;
}

/*
 *  stress_ptrace()
 *	stress ptracing
 */
int stress_ptrace(args_t *args)
{
	pid_t pid;

	pid = fork();
	if (pid < 0) {
		pr_fail_dbg("fork");
		return EXIT_FAILURE;
	} else if (pid == 0) {
		(void)setpgid(0, pgrp);
		stress_parent_died_alarm();

		/*
		 * Child to be traced, we abort if we detect
		 * we are already being traced by someone else
		 * as this makes life way too complex
		 */
		if (ptrace(PTRACE_TRACEME) != 0) {
			pr_fail("%s: ptrace child being traced "
				"already, aborting\n", args->name);
			_exit(0);
		}
		/* Wait for parent to start tracing me */
		kill(getpid(), SIGSTOP);

		/*
		 *  A simple mix of system calls
		 */
		while (opt_do_run) {
			(void)getppid();
			(void)getgid();
			(void)getegid();
			(void)getuid();
			(void)geteuid();
			(void)getpgrp();
			(void)time(NULL);
		}
		_exit(0);
	} else {
		/* Parent to do the tracing */
		int status;

		(void)setpgid(pid, pgrp);

		if (waitpid(pid, &status, 0) < 0) {
			if (errno != EINTR) {
				pr_fail_dbg("waitpid");
				return EXIT_FAILURE;
			}
			return EXIT_SUCCESS;
		}
		if (ptrace(PTRACE_SETOPTIONS, pid,
			0, PTRACE_O_TRACESYSGOOD) < 0) {
			pr_fail_dbg("ptrace");
			return EXIT_FAILURE;
		}

		do {
			/*
			 *  We do two of the following per syscall,
			 *  one at the start, and one at the end to catch
			 *  the return.  In this stressor we don't really
			 *  care which is which, we just care about counting
			 *  them
			 */
			if (stress_syscall_wait(args, pid))
				break;
			inc_counter(args);
		} while (opt_do_run && (!args->max_ops || *args->counter < args->max_ops));

		/* Terminate child */
		(void)kill(pid, SIGKILL);
		if (waitpid(pid, &status, 0) < 0)
			pr_fail_dbg("waitpid");
	}
	return EXIT_SUCCESS;
}
#else
int stress_ptrace(args_t *args)
{
	return stress_not_implemented(args);
}
#endif
