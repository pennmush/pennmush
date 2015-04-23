import os
import subprocess
import sys


def _ParseDeps(base_dir):
  """Returns a tuple of (deps, hooks)."""
  f = open(os.path.join(base_dir, "win32", "DEPS"))
  global_context = {}
  local_context = {}
  exec(f.read(), global_context, local_context)
  return local_context.get("deps", {}), local_context.get("hooks", [])


def _DoUpdate(base_dir, deps):
  """Updates checkout dependencies."""
  # FIXME: Ideally should automatically clean up removed dependencies.
  for path, repo in deps.iteritems():
    if os.path.isdir(os.path.join(base_dir, path)):
      os.chdir(os.path.join(base_dir, path))
      subprocess.call(["git", "pull"])
    else:
      os.chdir(base_dir)
      subprocess.call(["git", "clone", repo, path])


def _DoRunHooks(base_dir, hooks):
  """Runs post update hooks. Typically used for generating build files, etc."""
  pass


def _Main(argv):
  """Does stuff."""
  base_dir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
  deps, hooks = _ParseDeps(base_dir)
  if argv[1] == "init":
    _DoUpdate(base_dir, deps)
    _DoRunHooks(base_dir, hooks)
    _DoFirstTimeInfo()
  elif argv[1] == "update":
    _DoUpdate(base_dir, deps)
    _DoRunHooks(base_dir, hooks)
  elif argv[1] == "runhooks":
    _DoRunHooks(base_dir, hooks)
  else
    # FIXME: Print out some help.
    pass


if "__main__" == __name__:
  try:
    result = _Main(sys.argv)
  except Exception, e:
    print "Error: %s" % str(e)
    result = 1
  sys.exit(result)
