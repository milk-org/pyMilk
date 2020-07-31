import time
from threading import Thread


class ThreadHandler:
    '''
        Basic class template for performing looped operations going on forever,
        piloted from an interactive shell.

        self.threadPause and self.threadRun pause/restart the underlying self.thread

        self._t_runThread is the looped operation, which seeks the internal flags
        handling the start/stop/pause

        ----------
        self.threadCall is the portion executed when not paused

        That's what you should implement in subclasses using this template
        ----------
    '''

    def __init__(self, sleep_ms: float = 10, daemonize: bool= True) -> None:

        self._t_pauseSleepMs = sleep_ms
        self._t_threadSuspend = True
        self._t_threadDie = False

        self.thread = None  # type: Thread

        self._t_initThread(daemonize)

    def _t_initThread(self, daemonize) -> None:
        try:
            self.thread.join()
        except:
            pass
        self.thread = Thread(target=self._t_runThread)
        self.thread.start()
        self.thread.setDaemon(daemonize)

    def threadPause(self) -> None:
        self._t_threadSuspend = True

    def threadRun(self) -> None:
        self._t_threadSuspend = False

    def __del__(self) -> None:
        self._t_threadSuspend = True
        self._t_threadDie = True
        self.thread.join()
        print('ThreadHandler.__del__: Destruct OK')

    def _t_runThread(self) -> None:
        while True:
            if self._t_threadDie:
                return
            if self._t_threadSuspend:
                time.sleep(self._t_pauseSleepMs / 1000.)
            else:
                self.threadCall()

    def threadCall(self) -> None:
        raise NotImplementedError(
                'ThreadHandler.threadCall: This method must be overloaded.')
