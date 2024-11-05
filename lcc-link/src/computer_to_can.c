#include "computer_to_can.h"
#include "lcc-gridconnect.h"

K_THREAD_STACK_DEFINE(tx_stack, 512);
K_THREAD_STACK_DEFINE(rx_stack, 512);

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

static void computer_to_can_parse_data(struct computer_to_can* computer_to_can){
	static struct can_frame zephyr_can_frame_tx;
	static struct lcc_can_frame lcc_rx;
	char byte;

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
			// We have a full frame.  Parse and send out to the CAN bus
			int stat = lcc_gridconnect_to_canframe(computer_to_can->gridconnect_in, &lcc_rx);
			computer_to_can->gridconnect_in_pos = 0;
			if(stat == LCC_OK){
				// Push out to the CAN bus
				memset(&zephyr_can_frame_tx, 0, sizeof(zephyr_can_frame_tx));
				zephyr_can_frame_tx.id = lcc_rx.can_id;
				zephyr_can_frame_tx.dlc = lcc_rx.can_len;
				zephyr_can_frame_tx.flags = CAN_FRAME_IDE;
				memcpy(zephyr_can_frame_tx.data, lcc_rx.data, 8);

				k_msgq_put(&computer_to_can->tx_msgq, &zephyr_can_frame_tx, K_NO_WAIT);
			}
		}else if(byte == ';'){
			computer_to_can->gridconnect_in_pos = 0;
		}
	}
}

static void gridconnect_frame_parse_thread(void *arg1, void *unused2, void *unused3)
{
	struct computer_to_can* computer_to_can = arg1;

	// Continually try to parse frames that have come over USB
	while(1){
		if(k_sem_take(&computer_to_can->parse_semaphore, K_FOREVER) != 0){
			printf("Can't take sem??\n");
			continue;
		}

		// Parse everything in our buffer
		computer_to_can_parse_data(computer_to_can);
	}
}

void computer_to_can_init(struct computer_to_can* computer_to_can, const struct device* can_dev){
	memset(computer_to_can->gridconnect_in, 0, sizeof(computer_to_can->gridconnect_in));
	computer_to_can->gridconnect_in_pos = 0;
	computer_to_can->can_dev = can_dev;

	ring_buf_init(&computer_to_can->ringbuf_incoming,
			sizeof(computer_to_can->ring_buffer_incoming),
			computer_to_can->ring_buffer_incoming);
	k_sem_init(&computer_to_can->parse_semaphore, 0, 1);
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

	computer_to_can->rx_thread_tid = k_thread_create(&computer_to_can->rx_thread_data,
			rx_stack,
			K_THREAD_STACK_SIZEOF(rx_stack),
			gridconnect_frame_parse_thread, computer_to_can, NULL, NULL,
			0, 0,
			K_NO_WAIT);
	if (!computer_to_can->rx_thread_tid) {
		printf("ERROR spawning rx thread\n");
	}
}
