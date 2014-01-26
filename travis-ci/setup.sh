sudo apt-get -qq update
sudo apt-get -qq -y install openjdk-7-jdk ant lib32z1-dev lib32stdc++6
wget http://dl.google.com/android/android-sdk_r22.3-linux.tgz
wget http://dl.google.com/android/ndk/android-ndk-r9c-linux-x86_64.tar.bz2
tar xf android-sdk_r22.3-linux.tgz
tar xf android-ndk-r9c-linux-x86_64.tar.bz2
mv android-ndk-r9c android-ndk