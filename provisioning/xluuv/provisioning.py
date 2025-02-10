import subprocess
import time

from xluuv.components.component import ProvisionComponent
from xluuv.tmux import TmuxInterface, TmuxJobManager


class ProvisionManager:
    def __init__(self, components: list[ProvisionComponent], verbose: bool = False):
        self.tmux = TmuxInterface(verbose)
        self.manager = TmuxJobManager(self.tmux, verbose)
        self.components = components
        self.verbose = verbose

    def start_vms(self):
        for component in self.components:
            self.add_start_cmds(component)
        self.manager.run_jobs()

    def provision_vms_sequentially(self, rebuild: bool):
        session = "provisioning"
        self.tmux.create_session(session)
        for component in self.components:
            print(f"*** Provisioning {component.name}")
            commands = component.provision_commands(rebuild) + component.stop_commands()
            for command in commands:
                if command.blocking:
                    self.tmux.run_command_blocking(session, command)
                else:
                    self.tmux.run_command(session, command)
                    time.sleep(1)
        self.tmux.close_session(session)
        print("*** Provisioning successfully finished")

    def wait_until_vms_have_started(self):
        while True:
            queues_are_empty = all(
                [queue.qsize() == 0 for queue in self.manager.session_queues.values()]
            )
            threads_are_non_blocking = not any(
                [blocking for blocking in self.tmux.blocking.values()]
            )
            if queues_are_empty and threads_are_non_blocking:
                break
            time.sleep(1)
        print("*** VM startup has finished ...")

    def exec_post_start_cmds(self):
        for component in self.components:
            self.add_post_cmds(component)
        self.manager.run_jobs()

    def add_start_cmds(self, component: ProvisionComponent):
        self.tmux.create_session(component.name)
        print(f"*** Starting {component.name}-VM")
        for command in component.start_commands():
            self.manager.add_job(session_name=component.name, command=command)

    def add_post_cmds(self, component: ProvisionComponent):
        for command in component.post_start_commands():
            self.manager.add_job(session_name=component.name, command=command)

    def stop_vms(self):
        self.delete_external_sessions()
        self.stop_components()

    def delete_external_sessions(self):
        print("*** Remove remaining tmux sessions")
        to_delete = [component.name for component in self.components]
        for session in to_delete:
            command = f"tmux kill-session -t {session}"
            subprocess.run(command, shell=True)

    def stop_components(self):
        for component in self.components:
            print(f"*** Shut down {component.name}-VM")
            subprocess.run("vagrant halt", cwd=component.path, shell=True)
