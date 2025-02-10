import datetime
import time
import threading
import queue
import libtmux
from secrets import token_hex

from xluuv.components.component import Command


class TmuxInterface:
    def __init__(self, verbose: bool = False):
        self.server = libtmux.Server()
        self.sessions: dict[str, libtmux.Session] = {}
        self.verbose = verbose
        self.blocking: dict[str, bool] = {}

    def create_session(self, session_name) -> None:
        session = self.server.new_session(session_name=session_name, kill_session=False)
        self.sessions[session_name] = session

    def run_command(self, session_name, command: Command):
        pane = self.get_pane(session_name)
        pane.send_keys(command.cmd)
        self.debug_show_cmd_exec(command, session_name)

    def run_command_blocking(self, session_name, command: Command):
        pane = self.get_pane(session_name)
        flag = "TMUX_CMD_" + token_hex(16)
        echo_flag = f" && echo '{flag}'"
        pane.send_keys(command.cmd + echo_flag)
        self.debug_show_cmd_exec(command, session_name)
        self.blocking[session_name] = True
        self.wait_until_command_finished(command, pane, session_name, flag)

    def wait_until_command_finished(
        self, command: Command, pane: libtmux.window.Pane, session_name: str, flag: str
    ):
        while self.blocking[session_name]:
            output = pane.capture_pane()
            if flag in output:
                self.debug_show_cmd_exec(command, session_name)
                self.blocking[session_name] = False

    def debug_show_cmd_exec(self, command, session_name):
        if self.verbose:
            print(
                f"*** DEBUG ({datetime.datetime.now()}) Execute command [{session_name}] >>> {command}"
            )

    def get_pane(self, session_name) -> libtmux.window.Pane:
        session = self.find_session(session_name)
        window = session.attached_window
        pane = window.attached_pane
        return pane

    def close_session(self, session_name: str) -> None:
        session = self.find_session(session_name)
        session.kill_session()

    def find_session(self, session_name: str) -> libtmux.Session:
        session = self.sessions.get(session_name)
        if session is None:
            raise ValueError(f'*** DEBUG Session "{session_name}" not found.')
        return session


class TmuxJobManager:
    def __init__(self, iface: TmuxInterface = None, verbose: bool = False):
        if iface is None:
            self.iface = TmuxInterface()
        else:
            self.iface = iface
        self.session_queues = {}
        self.verbose = verbose
        self.non_blocking_timeout = 1

    def _worker(self, session_name, command: Command):
        self.show_debug_info(session_name)
        try:
            if command.blocking:
                self.iface.run_command_blocking(session_name, command)
            else:
                self.iface.run_command(session_name, command)
                time.sleep(self.non_blocking_timeout)
        except Exception as e:
            print(f"[+] ERROR executing '{command}' in session '{session_name}': {e}")
            exit(1)

    def show_debug_info(self, session_name: str):
        if self.verbose:
            print(
                f"*** DEBUG ({datetime.datetime.now()})\tQueue size for {session_name}: "
                f"{self.session_queues[session_name].qsize()}"
            )

    def add_job(self, session_name, command: Command):
        if session_name not in self.session_queues:
            self.session_queues[session_name] = queue.Queue()

        self.session_queues[session_name].put(command)

    def run_jobs(self):
        for session_name, _ in self.session_queues.items():
            if hasattr(self, f"_thread_{session_name}"):
                if (
                    self.iface.blocking[session_name]
                    or self.session_queues[session_name].qsize() == 0
                ):
                    print(
                        "ERROR: `run_jobs` is executed twice, although the old jobs have not finished."
                    )
                    exit(1)
            setattr(
                self,
                f"_thread_{session_name}",
                threading.Thread(target=self._process_queue, args=(session_name,)),
            )
            getattr(self, f"_thread_{session_name}").start()

    def _process_queue(self, session_name):
        while True:
            try:
                command = self.session_queues[session_name].get_nowait()
            except queue.Empty:
                break

            self._worker(session_name, command)
            self.session_queues[session_name].task_done()
