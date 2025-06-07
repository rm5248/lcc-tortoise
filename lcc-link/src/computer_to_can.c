#include "computer_to_can.h"
#include "lcc-gridconnect.h"

K_THREAD_STACK_DEFINE(tx_stack, 512);
K_THREAD_STACK_DEFINE(parse_stack, 512);

static int computer_to_can_parse(struct computer_to_can* comp_to_can, struct can_frame* frame);

static void can_frame_send_thread(void *arg1, void *unused2, void *unused3)
{
	static struct can_frame tx_frame;
	struct computer_to_can* computer_to_can = arg1;
	struct can_bus_err_cnt err_cnt = {0, 0};
	enum can_state state;

	while(1){
		if(k_msgq_get(&computer_to_can->tx_msgq, &tx_frame, K_FOREVER) == 0){
			int err = can_get_state(computer_to_can->can_dev, &state, &err_cnt);
			if(err != 0){
				printf("Can't get CAN state\n");
				continue;
			}

			if(state == CAN_STATE_BUS_OFF){
				printf("CAN bus off, dropping packet\n");
				continue;
			}

			int stat = can_send(computer_to_can->can_dev, &tx_frame, K_FOREVER, NULL, NULL );
			if(stat < 0){
				printf("Unable to send: %d\n", stat);
			}
		}
	}
}

static void computer_to_can_parse_thread(void *arg1, void *unused2, void *unused3){
	struct computer_to_can* computer_to_can = arg1;
	struct can_frame frame;

	while(1){
		k_sem_take(&computer_to_can->parse_sem, K_FOREVER);
		int parsed = computer_to_can_parse(computer_to_can, &frame);
		if(parsed){
			if(k_msgq_put(&computer_to_can->parsed_msgq, &frame, K_NO_WAIT) < 0){
				printf("Overflow parsed message queue!\n");
			}
			// Give the semaphore back so that we try to parse a frame again.
			// If we can't parse a full frame, we effectively don't do anything.
			k_sem_give(&computer_to_can->parse_sem);
		}
	}
}

void computer_to_can_init(struct computer_to_can* computer_to_can, const struct device* can_dev){
	memset(computer_to_can->gridconnect_in, 0, sizeof(computer_to_can->gridconnect_in));
	computer_to_can->gridconnect_in_pos = 0;
	computer_to_can->can_dev = can_dev;

	ring_buf_init(&computer_to_can->ringbuf_incoming,
			sizeof(computer_to_can->ring_buffer_incoming),
			computer_to_can->ring_buffer_incoming);
	k_msgq_init(&computer_to_can->tx_msgq, computer_to_can->tx_msgq_data, sizeof(struct can_frame), 55);

	computer_to_can->tx_thread_tid = k_thread_create(&computer_to_can->tx_thread_data,
			tx_stack,
			K_THREAD_STACK_SIZEOF(tx_stack),
			can_frame_send_thread, computer_to_can, NULL, NULL,
			0, 0,
			K_NO_WAIT);
	if (!computer_to_can->tx_thread_tid) {
		printf("ERROR spawning tx thread\n");
	}

	k_msgq_init(&computer_to_can->parsed_msgq, computer_to_can->parsed_frames, sizeof(struct can_frame), 12);
	k_sem_init(&computer_to_can->parse_sem, 0, 1);
	computer_to_can->parse_thread_tid = k_thread_create(&computer_to_can->parse_thread_data,
			parse_stack,
			K_THREAD_STACK_SIZEOF(parse_stack),
			computer_to_can_parse_thread, computer_to_can, NULL, NULL,
			0, 0,
			K_NO_WAIT);
	if (!computer_to_can->parse_thread_tid) {
		printf("ERROR spawning parse thread\n");
	}
}

int computer_to_can_append_data(struct computer_to_can* comp_to_can, void* data, int len){
	int rb_len;

	rb_len = ring_buf_put(&comp_to_can->ringbuf_incoming, data, len);

	k_sem_give(&comp_to_can->parse_sem);

	return rb_len;
}

int computer_to_can_parse(struct computer_to_can* computer_to_can, struct can_frame* frame){
	char byte;
	static struct lcc_can_frame lcc_rx;

	// Rather stupid, but we will read bytes 1-by-1 until we have a full packet, discarding bytes
	// that are invalid.
	while(!ring_buf_is_empty(&computer_to_can->ringbuf_incoming)){
		ring_buf_get(&computer_to_can->ringbuf_incoming, &byte, 1);
		if(byte == ':'){
			computer_to_can->gridconnect_in_pos = 0;
		}

		computer_to_can->gridconnect_in[computer_to_can->gridconnect_in_pos] = byte;
		computer_to_can->gridconnect_in_pos++;

		if(computer_to_can->gridconnect_in_pos > sizeof(computer_to_can->gridconnect_in)){
			computer_to_can->gridconnect_in_pos = 0;
		}

		if(byte == ';' && computer_to_can->gridconnect_in_pos > 0){
			// We have a full frame.  Parse and return the value
			int stat = lcc_gridconnect_to_canframe(computer_to_can->gridconnect_in, &lcc_rx);
			computer_to_can->gridconnect_in_pos = 0;
			if(stat == LCC_OK){
				// Convert into a zephyr CAN frame
				memset(frame, 0, sizeof(struct can_frame));
				frame->id = lcc_rx.can_id;
				frame->dlc = lcc_rx.can_len;
				frame->flags = CAN_FRAME_IDE;
				memcpy(frame->data, lcc_rx.data, 8);
				return 1;
			}
			return -1;
		}else if(byte == ';'){
			computer_to_can->gridconnect_in_pos = 0;
		}
	}

	return 0;
}

void computer_to_can_tx_frame(struct computer_to_can* comp_to_can, struct can_frame* frame){
	k_msgq_put(&comp_to_can->tx_msgq, frame, K_NO_WAIT);
}

struct k_msgq* computer_to_can_parsed_queue(struct computer_to_can* comp_to_can){
	return &comp_to_can->parsed_msgq;
}
