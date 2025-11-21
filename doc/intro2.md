# 1.2 Overview of the different projects (MainServer, CastServer, AuthServer, Common, Client)

Before introducing the requirements and the installation process, let's first discuss quickly what the different sub-projects of this project are, and some important stuff that you may notice once you have your setup ready.

<a href="https://ibb.co/WWSVc6Ky"><img src="https://i.ibb.co/N6wTLsYj/server-setup.webp" alt="server-setup" border="0" /></a>

## Common
This is a static library (.lib) that includes shared utilities used by all the servers: Auth, Main, and Cast.

It acts as the foundation for many core features like networking (handling communication between the server and client), cryptography, CDB data structures, and other essential components that are reused across the project.


## Auth Server
As the name suggests, this is the server responsible for handling user authentication — basically, the login process.

It connects with any MainServer defined in the config file (we’ll go over this setup later) to fetch the current player count from each server. These counts are shown in the channel list during login.

**⚠️ An important note**: accounts with elevated privileges (Moderator and above) are required to have 2FA enabled for security reasons.
This setting can be configured directly in the database (we’ll cover how later), but you’re also free to modify the AuthServer code if you want to disable this requirement.


## Main Server
The MainServer is at the core of the emulator - it handles just about everything except login and most of the gameplay logic.

In simple terms, it’s responsible for:

- Managing player state
- Handling inventory, shop, capsule, and item systems
- Managing trades between players
- Creating and joining rooms, as well as in-game chat
- Saving user data to the database
...and much more!

As you can probably tell, this is where most of the heavy lifting happens. Important data is encrypted here too.

The main server communicates with the Auth server as explained above, but also with the Cast server. This communication is needed to give the cast server some more information that it misses, 
such as the match's map ID and match mode.

  ## Cast Server
The CastServer is responsible for handling most of the gameplay-related communication. Basically, anything that happens inside a match is managed here, such as:

- Weapon damage 
- Player respawns
- Match state and updates
- Item pickups and drops
- Player position and movement updates
...and more.

Since this server handles real-time gameplay, it needs to be fast. For that reason, the packets sent here are not encrypted - unlike the MainServer, which deals with more sensitive data and uses encryption accordingly.

## Client folder
This is where we added a few utilities to help with client modifications. More details on how to use them are covered in a later section.

## External Libraries
This folder holds all the external dependencies used across the project, including header-only libraries.

When you compile the Common project, it will generate a .lib file that gets placed here under /ExternalLibraries/CommonLib.

**⚠️ Important:**: Make sure to place the unpacked `cgd.dip` archive inside the cgd_original folder here.
This archive is essential for the Main Server and contains important client data like:

- Shop item prices
- Capsule contents
- Available inventory items
...and so on.

Without it, the server won’t be able to function properly when handling anything related to (for example) game items, capsule info, etc. 
So always make sure you’re using the most up-to-date and correctly unpacked version of `cgd.dip`.

**Note**: This repository already provides a server-sided `cgd_original` folder, along with the corresponding client `cgd.dip` which it was extracted from. All the links to the client & `cgd.dip` folder are given in the next chapters.

## Next
[2.1 Changelog from version 1.0 to version 2.0](https://github.com/SoWeBegin/MicrovoltsEmulator/blob/mv1.1_2.0/doc/changelog1.md)
