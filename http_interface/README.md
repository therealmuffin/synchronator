# Synchronator HTTP Interface
Synchronator HTTP Interface is a Synchronator daemon companion application adding an http 
interface for synchronator. This allows some basic control over Synchronator controlled 
devices (amplifier), such as power, input, and some basic macro's.

## Requirements
- http server with php on the same server as synchronator daemon.
- running instance of the synchronator daemon.

## Installation
- Place both php files in a location accessible via your http server.

## Configuration
- configure interface via the configuration file: synchronator.config.php.
- read the configuration file for instructions.