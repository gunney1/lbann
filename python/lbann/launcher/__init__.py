import os, os.path
import datetime
import lbann, lbann.proto
import lbann.launcher.slurm

# ==============================================
# Run experiments
# ==============================================

def run(lbann_exe, model, data_reader, optimizer,
        lbann_args = '',
        experiment_dir = None,
        nodes = 1,
        procs_per_node = 1,
        time_limit = 60,
        scheduler = 'slurm',
        job_name = 'lbann',
        system = None,
        partition = None,
        account = None,
        launcher_args = '',
        environment = {},
        setup_only = False):
    """Run LBANN experiment.

    This is intended to interface with job schedulers on HPC
    clusters. It will either submit a batch job (if on a login node)
    or run with an existing node allocation (if on a compute
    node). Behavior may vary across schedulers.

    If an experiment directory is not provided, a timestamped
    directory is created (by default in `lbann/experiments`). The
    location of autogenerated experiment directories can be set with
    the environment variable `LBANN_EXPERIMENT_DIR`.

    Args:
        lbann_exe (str): LBANN executable.
        model (lbann.model.Model or lbann_pb2.Model): Neural network
            model.
        data_reader (lbann_pb2.DataReader): Data reader.
        optimizer (lbann.model.Model or lbann_pb2.Optimizer): Default
            optimizer for model.
        lbann_args (str, optional): Command-line arguments to LBANN
            executable.
        experiment_dir (str, optional): Experiment directory.
        nodes (int, optional): Number of compute nodes.
        procs_per_node (int, optional): Number of processes per compute
            node.
        time_limit (int, optional): Job time limit, in minutes.
        scheduler (str, optional): Job scheduler.
        job_name (str, optional): Batch job name.
        system (str, optional): Target system.
        partition (str, optional): Scheduler partition.
        account (str, optional): Scheduler account.
        launcher_args (str, optional): Command-line arguments to
            launcher.
        environment (dict of {str: str}, optional): Environment
            variables.
        setup_only (bool, optional): If true, the experiment is not
            run after the experiment directory is initialized.

    """

    # Construct experiment directory if needed
    if not experiment_dir:
        experiment_dir = os.path.join(lbann.lbann_dir(), 'experiments')
        if 'LBANN_EXPERIMENT_DIR' in os.environ:
            experiment_dir = os.environ['LBANN_EXPERIMENT_DIR']
        timestamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
        experiment_dir = os.path.join(experiment_dir,
                                '{}_{}'.format(timestamp, job_name))
        i = 1
        while os.path.lexists(experiment_dir):
            i += 1
            experiment_dir = os.path.join(
                os.path.dirname(experiment_dir),
                '{}_{}_{}'.format(timestamp, job_name, i))
    experiment_dir = os.path.abspath(experiment_dir)
    os.makedirs(experiment_dir, exist_ok=True)

    # Create experiment prototext file
    prototext_file = os.path.join(experiment_dir, 'experiment.prototext')
    lbann.proto.save_prototext(prototext_file,
                               model = model,
                               data_reader = data_reader,
                               optimizer = optimizer)
    lbann_args += ' --prototext=' + prototext_file

    # Run experiment
    if scheduler.lower() in ('slurm', 'srun', 'sbatch'):
        slurm.run(experiment_dir = experiment_dir,
                  command = '{} {}'.format(lbann_exe, lbann_args),
                  nodes = nodes,
                  procs_per_node = procs_per_node,
                  time_limit = time_limit,
                  job_name = job_name,
                  partition = partition,
                  account = account,
                  srun_args = launcher_args,
                  environment = environment,
                  setup_only = setup_only)
    else:
        raise RuntimeError('unsupported job scheduler ({})'
                           .format(scheduler))
