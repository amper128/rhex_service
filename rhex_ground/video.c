/**
 * @file video.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Получение видеопотока
 */

#include <svc/svc.h>

#include <wfb/wfb_rx_rawsock.h>
#include <wfb/wfb_status.h>

#include <private/video.h>

wifibroadcast_rx_status_t *rx_status = NULL;

typedef struct {
	int stdout_fds[2];
	int stdin_fds[2];
	int stderr_fds[2];
	pid_t pid;
} gst_desc_t;

static int
gstreamer_start(gst_desc_t *gst_desc)
{
	int result = 0;

	do {
		result = pipe(gst_desc->stdout_fds);
		if (result < 0) {
			log_err("pipe() error: %i", result);
			break;
		}

		result = pipe(gst_desc->stdin_fds);
		if (result < 0) {
			log_err("pipe() error: %i", result);
			break;
		}

		result = pipe(gst_desc->stderr_fds);
		if (result < 0) {
			log_err("pipe() error: %i", result);
			break;
		}

		gst_desc->pid = fork();
		if (gst_desc->pid < 0) {
			result = -1;
			log_err("fork() error: %i", -gst_desc->pid);
			break;
		}

		if (gst_desc->pid == 0) {
			/* we are a new process */
			if (dup2(gst_desc->stdin_fds[0], 0) < 0) {
				log_err("dup2(stdin) error");
				_exit(127);
			}

			if (dup2(gst_desc->stdout_fds[1], 1) < 0) {
				log_err("dup2(stdout) error");
				_exit(127);
			}

			if (dup2(gst_desc->stderr_fds[1], 2) < 0) {
				log_err("dup2(stderr) error");
				_exit(127);
			}

			/* closing other side of pipes */
			close(gst_desc->stdin_fds[1]);
			close(gst_desc->stdout_fds[0]);
			close(gst_desc->stderr_fds[0]);

			/* run program */
			static const char *args_list[] = {
			    /* progname */
			    "/usr/bin/gst-launch-1.0",
			    /* src */
			    "fdsrc",
			    /* pipe */
			    "!",
			    /* parse */
			    "h264parse",
			    /* pipe */
			    "!",
			    /* rtp */
			    "rtph264pay", "pt=96", "config-interval=1",
			    /* pipe */
			    "!",
			    /* sink */
			    "udpsink", "port=5600", "host=192.168.0.30",
			    /*"avdec_h264", "!", "autovideosink",*/
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
		close(gst_desc->stdout_fds[0]);
		close(gst_desc->stdout_fds[1]);
		close(gst_desc->stdin_fds[0]);
		close(gst_desc->stdin_fds[1]);
		close(gst_desc->stderr_fds[0]);
		close(gst_desc->stderr_fds[1]);
	}

	return result;
}

int
video_init(void)
{
	return 0;
}

int
video_main(void)
{
	log_dbg("starting video rx");
	// setpriority(PRIO_PROCESS, 0, -10);

	static wfb_rx_stream_t stream;

	// rx_status = status_memory_open();

	gst_desc_t gst;

	gstreamer_start(&gst);

	wfb_rx_stream_init(&stream, 0);

	while (svc_cycle()) {
		wfb_rx_stream_packet_t rx_data;
		rx_data.bytes = 0;
		wfb_rx_stream(&stream, &rx_data);

		if (rx_data.bytes > 0) {
			int r;
			r = write(gst.stdin_fds[1], rx_data.data, (size_t)rx_data.bytes);
			(void)r;
		}
	}

	log_dbg("video exit");

	return 0;
}
