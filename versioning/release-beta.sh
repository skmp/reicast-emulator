echo "Make sure repo is up to date"
git checkout origin/alpha
git tag -a $1 -m "Reicast $1\n---\n`./changelog-beta.sh`"
git checkout beta
git reset --hard origin/alpha
echo "You need to do:"
echo "git push origin $1:$1 # push tag"
echo "git push # push beta branch"
