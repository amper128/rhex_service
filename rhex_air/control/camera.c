/**
 * @file camera.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Передача видеопотока
 */

#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#include <log/log.h>
#include <svc/platform.h>
#include <svc/svc.h>
#include <wfb/wfb_tx_rawsock.h>

#include <private/camera.h>

typedef struct {
	int stdout_fds[2];
	int stdin_fds[2];
	int stderr_fds[2];
	pid_t pid;
} camera_desc_t;

static int
camera_start(camera_desc_t *cam_desc)
{
	int result = 0;

	do {
		result = pipe(cam_desc->stdout_fds);
		if (result < 0) {
			log_err("pipe() error: %i", result);
			break;
		}

		result = pipe(cam_desc->stdin_fds);
		if (result < 0) {
			log_err("pipe() error: %i", result);
			break;
		}

		result = pipe(cam_desc->stderr_fds);
		if (result < 0) {
			log_err("pipe() error: %i", result);
			break;
		}

		cam_desc->pid = fork();
		if (cam_desc->pid < 0) {
			result = -1;
			log_err("fork() error: %i", -cam_desc->pid);
			break;
		}

		if (cam_desc->pid == 0) {
			/* we are a new process */
			if (dup2(cam_desc->stdin_fds[0], 0) < 0) {
				log_err("dup2(stdin) error");
				_exit(127);
			}

			if (dup2(cam_desc->stdout_fds[1], 1) < 0) {
				log_err("dup2(stdout) error");
				_exit(127);
			}

			if (dup2(cam_desc->stderr_fds[1], 2) < 0) {
				log_err("dup2(stderr) error");
				_exit(127);
			}

			/* closing other side of pipes */
			close(cam_desc->stdin_fds[1]);
			close(cam_desc->stdout_fds[0]);
			close(cam_desc->stderr_fds[0]);

			/* run program */
			static const char *args_list[] = {/* progname */
							  "/usr/bin/raspivid",
							  /* width */
							  "-w", "1920",
							  /* height */
							  "-h", "1080",
							  /* fps */
							  "-fps", "30",
							  /* bitrate */
							  "-b", "6000000",
							  /* keyframe rate */
							  "-g", "10",
							  /* start immediate */
							  "-t", "0",
							  /* codec */
							  "-cd", "H264",
							  /* no preview */
							  "-n",
							  /* flush */
							  "-fl",
							  /* inline headers */
							  "-ih",
							  /* h.264 profile */
							  "-pf", "high",
							  /* intra refresh */
							  "-if", "both",
							  /* exposure */
							  "-ex", "sports",
							  /* metering mode */
							  "-mm", "average",
							  /* AWB mode */
							  "-awb", "horizon",
							  /* output to stdout */
							  "-o", "-",
							  /* end */
							  NULL};

			/* discard const qualifier workaround */
			union {
				const char **cargp;
				char **const argp;
			} argv;
			argv.cargp = args_list;

			execv(argv.argp[0], argv.argp);

			log_err("cannot execv: %i", errno);
			_exit(-1);
		}

		result = 0;
	} while (false);

	if (result < 0) {
		/* cleanup */
		close(cam_desc->stdout_fds[0]);
		close(cam_desc->stdout_fds[1]);
		close(cam_desc->stdin_fds[0]);
		close(cam_desc->stdin_fds[1]);
		close(cam_desc->stderr_fds[0]);
		close(cam_desc->stderr_fds[1]);
	}

	return result;
}

static int
camera_cycle(camera_desc_t *cd, wfb_stream_t *wfb_stream)
{
	int result = 0;

	do {
		/* check what raspivid still alive */
		if (waitpid(cd->pid, NULL, WNOHANG) == cd->pid) {
			log_warn("raspivid process killed");
			break;
		}

		struct timeval to;
		/* 1s timeout */
		to.tv_sec = 1;
		to.tv_usec = 0;
		fd_set readset;
		FD_ZERO(&readset);

		int nfds = cd->stdout_fds[0];
		FD_SET(cd->stdout_fds[0], &readset);
		FD_SET(cd->stderr_fds[0], &readset);
		if (cd->stderr_fds[0] > nfds) {
			nfds = cd->stderr_fds[0];
		}

		int n;
		n = select(nfds, &readset, NULL, NULL, &to);

		if (n < 0) {
			log_err("select() error");
			result = -1;
			break;
		}

		if (n == 0) {
			/* no data */
			break;
		}

		/* we have a data in stderr */
		if (FD_ISSET(cd->stderr_fds[0], &readset)) {
			char buf[1024U];

			int r = read(cd->stderr_fds[0], buf, sizeof(buf));
			if (r < 0) {
				log_err("read() error");
				result = -1;
				break;
			}

			log_inf("raspivid: %.*s", r, buf);
		}

		/* we have a stream data */
		if (FD_ISSET(cd->stdout_fds[0], &readset)) {
			static uint8_t tmp_buf[MAX_PACKET_LENGTH];
			int r = read(cd->stdout_fds[0], tmp_buf, MAX_PACKET_LENGTH);
			if (r < 0) {
				log_err("read() error");
				result = -1;
				break;
			}

			wfb_tx_stream(wfb_stream, tmp_buf, (uint16_t)r);
		}
	} while (false);

	return result;
}

int
camera_init(void)
{
	/* do nothing */

	return 0;
}

int
camera_main(void)
{
	int result = 0;

	do {
		wfb_stream_t wfb_stream;
		result = wfb_stream_init(&wfb_stream, 0, 1, false, false, false);
		if (result < 0) {
			break;
		}

		camera_desc_t cd;

		result = camera_start(&cd);
		if (result < 0) {
			break;
		}

		while (svc_cycle()) {
			if (camera_cycle(&cd, &wfb_stream) < 0) {
				break;
			}
		}

		kill(cd.pid, SIGKILL);
		wait(NULL);
	} while (false);

	return result;
}
