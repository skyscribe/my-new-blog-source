#!/bin/bash
deployDir=$(pwd)/_deploy
siteDir=$(pwd)/_site
siteUrl=https://github.com/skyscribe/skyscribe.github.io.git

function setup() {
    rm -fr $deployDir
    mkdir -p $deployDir
    cd "$deployDir"
    git init
    echo "dummy content" > index.html
    git add .
    git commit -m "dummy script init"
    git branch -m master
    git remote add origin "$siteUrl"
    echo "Setup complete for $deployDir"
}

function deploy() {
    bundle exec jekyll build
    cd "$deployDir"
    cp -r "$siteDir"/* "$deployDir/"
    git pull origin master
    git add -A
    now=$(date "+%Y-%m-%d_%H:%M:%S")
    git commit -m "site updated at $now"
    git push origin master --force
    echo "deploy completed"
}

case $1 in
  setup)
	    setup
        ;;

    * )
        deploy
        ;;
esac
