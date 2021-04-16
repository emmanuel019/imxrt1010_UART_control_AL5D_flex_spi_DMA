 This project is a C server that controls the AL5D robot arm, via UART communication between the board and the arm. 
As well as  between the board and the PC. This project can be compile and download to the board using MCUXpresso IDE.
This project is based on a NPX SDK.

Getting started: 

	  (1)   Clone the project 
	  (2)   Open the MCUXpresso IDE, importe the project using File -> Open Project from file system
	  (3)	Compile the project 
	  
	  (4)   HW Connection:
			 - connect the Tx pin (UART4 =>> J56 [4] to the Rx pin of the AL5D arm 
			 - Connect the board to a PC (for power supply and to flash the code)
			 
	  (6)	Flash the project (Debug session or Run session)
	  (7) 	Open a serial console terminal (9600 bps)
	  (8)   Use the keyboard to control the robot arm (using the help displayed on the console) 
	  (9)   Use special caractere underline in the guide display at the launch of the program to send the past
		control sequences to the QSPI via a DMA.
	
Brief presentation of the project :

The main application can be found in the source/app.c file :
the application decomposes into several major parts 

1.  the processing of commands received in USB, via a mapper, 
    the calculation of control sequences.

2.  The transmission in UART to the robot arm 
    (in blocking mode in order to have the most instantaneous control possible), 
     as well as the transmission in UART to the PC (for debugging, therefore this action can be asynchronous)
3.   Reception of data from the keyboard. The reception is done via Interrupt, and a mapper provide a conversion from caracter into control sequence. 

4.   Saving control sequence in the flash memory using a DMA for preserve the CPU Time

This project is not completely finished, 
bugs still exist but it provides a good basis for further development on the IMXRT1010 board, 
especially regarding interrupts, UART communication, DMA transfer to external memory

for more information please consult the document gettin_started_uart_flash_nor_transfert.docx



