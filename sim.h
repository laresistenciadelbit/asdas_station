Class Sim
{
	private:
		SoftwareSerial simSerial;
		char position[POSITION_SIZE];


		char buffer[BUFFER_SIZE];

		char post_msg_buffer[BUFFER_SIZE];

		char post_msg[BUFFER_SIZE];

#if defied(USE_SIM808)
		SIM808 sim;
#else
		SIM800L sim;
#endif


	public:
		Sim(byte , byte);
		void ();
		

}