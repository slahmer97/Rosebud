rel_subdir="pcie"
repo="git@github.com:alexforencich/verilog-pcie.git"
remote="pcie"
branch="master"
libname="PCIe"
seldir=""
rmlist=".test_durations README.md tox.ini .gitignore .github example scripts dma_block.svg README"
# commit="5f90e39e59b3bf5e9fd154f52ce8480746b6afb2"

# determine repo absolute path
if [ -n "$rel_subdir" ]; then
    # cd to script dir
    SOURCE="${BASH_SOURCE[0]}"
    while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
      DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
      SOURCE="$(readlink "$SOURCE")"
      [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE" # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
    done
    DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

    cd "$DIR"

    # relative path to script dir
    git-absolute-path () {
        fullpath=$(readlink -f "$1")
        gitroot="$(git rev-parse --show-toplevel)" || return 1
        [[ "$fullpath" =~ "$gitroot" ]] && echo "${fullpath/$gitroot\//}"
    }

    subdir="$(git-absolute-path .)/$rel_subdir"
fi

cd $(git rev-parse --show-toplevel)

git remote add -f $remote $repo
git checkout -b staging-branch $remote/$branch
if [ ! -z "$commit" ]; then
  git checkout $commit
fi
if [ ! -z "$seldir" ]; then
  git subtree split -P $seldir -b staging-branch2
  git checkout staging-branch2
else
  git branch staging-branch2
  git checkout staging-branch2
fi
if [ ! -z "$rmlist" ]; then
  git rm -rf $rmlist
  git commit -m "Prune as a lib."
fi
git checkout master
if [ ! -d "$subdir" ]; then
  git subtree add -P "$subdir" --squash staging-branch2 -m "Add $libname lib"
else
  git subtree merge -P "$subdir" --squash staging-branch2 -m "Merge for updating the $libname lib"
fi
git branch -D staging-branch staging-branch2
git remote rm $remote
