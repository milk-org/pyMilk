'''
zmq_send.py

Usage:
    zmq_send.py <host:port> <shm_name>

Options:
    <host:port>     IP adress : port to bind to.
    <shm_name>      SHMs to send
'''
''' TBC
    <shm_name>      One or more SHMs to send
    <trig_name>     Trig SHM name - mandatory if more than one SHM name
                    Must be a repeat of one of the SHM names
'''
'''bash
IP=""
for CH in 00 01 02 03 04 05 06 07 08 09 10 11; do
    zmq_send.py ${IP}:321${CH} dm00disp${CH}
done
'''

import zmq
import pickle
from typing import Tuple
from docopt import docopt
from pyMilk.interfacing.shm import SHM


def zmq_send_loop(host_port: Tuple[str, int], shm_name: str):

    # Open shared memories
    shm_obj = SHM(shm_name)

    # Get the ZMQ side ready
    context = zmq.Context()
    socket = context.socket(zmq.PUB)

    socket.bind(f"tcp://{host_port[0]}:{host_port[1]}")

    init = True
    while True:
        data = shm_obj.get_data(check=True, checkSemAndFlush=init, timeout=1.0)
        init = False
        kw = shm_obj.get_keywords()

        message = (shm_name + ' ').encode('ascii')
        message += pickle.dumps((kw, data))
        socket.send(message)


if __name__ == "__main__":
    # Parse
    from docopt import docopt
    doc = docopt(__doc__)

    hp = doc['<host:port>'].split(':')
    host = hp[0]
    port = int(hp[1])
    shm_name = doc['<shm_name>']

    zmq_send_loop((host, port), shm_name)
