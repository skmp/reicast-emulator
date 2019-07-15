echo "Make sure repo is up to date"
git checkout origin/alpha
git checkout beta
git reset --hard origin/alpha
echo "You need to do:"
echo "$ git push # push beta branch"
