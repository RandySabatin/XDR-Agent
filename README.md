# Windows XDR-Agent
![Alt text](image/XDR.png?raw=true "XDR")
The Windows XDR Agent, running as a service on endpoints, includes the following internal modules:
1. Event Log Handler  
   This module processes event data received from third-party programs, including:
	1. Sysmon Events
		- Captures system activity to help identify potentially malicious behavior
		- Events are mapped to MITRE ATT&CK tactics and techniques
		- You can find full details about Sysmon at: https://learn.microsoft.com/en-us/sysinternals/downloads/sysmon
	2. Windows Defender Events
		- Logs malware detection and response actions performed by Windows Defender
2. Command Handler  
   Handles various system and security-related commands, including:
	1. Retrieving system information from the endpoint
	2. Getting or setting Windows Defender configurations
	3. Triggering Windows Defender updates
	4. Initiating manual scans using Windows Defender
3. Protector Module  
   Provides security and integrity features, such as:
	1. Preventing the process from being terminated
	2. Protecting registry entries
	3. Securing specific files and folders
4. Sender / Receiver / Scheduler Module  
   Manages communication and task scheduling:
	1. Transmits and receives data from the central server
	2. Schedules command execution for the Command Handler
	3. Coordinates task and command dispatching  
