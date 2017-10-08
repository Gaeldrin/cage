namespace cage
{
	class CAGE_API discoveryClientClass
	{
	public:
		void update();
		void addServer(const string &address, uint16 port);
		uint32 peersCount() const;
		void peerData(uint32 index, string &message, string &address, uint16 &port) const;
	};

	class CAGE_API discoveryServerClass
	{
	public:
		void update();
		string message;
	};

	CAGE_API holder<discoveryClientClass> newDiscoveryClient(uint16 sendPort, uint32 gameId);
	CAGE_API holder<discoveryServerClass> newDiscoveryServer(uint16 listenPort, uint16 gamePort, uint32 gameId);
}
