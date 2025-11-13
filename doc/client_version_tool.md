# 7.2 Changing the client version (using the client version tool)

As new updates are pushed through the updater, one has the necessity to **force the players** to use the latest major client version. Otherwise, we risk having multiple players play on different client versions.

To fix this I decided to also create a very simple tool that patches the client's executable and forces it to use our specified client version (for example `0.0.1`).

Once you have patched the client with the tool, you will **also need to update the `config.ini` file inside the `Setup` folder**, which contains a `[Client]` and `ClientVersion=` line.

You will need to use **the same client version in the setup file**. For example, if you patched the client to let it use version `1.2.3`, you will also need to update the `config.ini` file to use `ClientVersion = 1.2.3`.

This will let the server know what the correct client version is, and it will only let players who have the correct client version join the server itself.

### Tool information & usage
The tool can be found inside `Tools/Client/VersionPatcher.cpp`. All you need is to compile it and obtain the executable.

Once you have the executable ready, start it and it will prompt you to enter the full path where your game client (`Microvolts.exe` or `ToyBattles.exe`) is, and the new version you want it to use.


## Next
[7.3 Changing the CGD password (using the password tool)](https://github.com/SoWeBegin/MicrovoltsEmulator/blob/mv1.1_2.0/doc/password_updater.md)
