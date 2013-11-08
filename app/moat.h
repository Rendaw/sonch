#ifndef moat_h
#define moat_h

template <typename InnerT> struct MoatT
{
	MoatT(void) {}
	template <typename... ArgumentsT> MoatT(ArgumentsT... Arguments) : Inner(std::forward<ArgumentsT>(Arguments)...) {}
	
	struct BridgeT
	{
		BridgeT(void) = delete;
		BridgeT(BridgeT const &Other) = delete;
		BridgeT &operator =(BridgeT const &Other) = delete;
		BridgeT &operator =(BridgeT &&Other) = delete;
		
		InnerT *operator ->(void) { return &Inner; }
		InnerT *operator ->(void) const { return &Inner; }
		
		private:
			friend class MoatT;
			BridgeT(BridgeT &&Other) : Inner(Other.Inner), Guard(std::move(Other.Guard)) {}
			BridgeT(InnerT &Inner) : Inner(Inner), Guard(Inner.Mutex) {}
			InnerT &Inner;
			std::lock_guard<std::mutex> Guard;
	};
	
	template <typename FunctionT> void Cross(FunctionT const &Function)
		{ Function(BridgeT(Inner)); }
		
	template <typename FunctionT> void operator ()(FunctionT const &Function)
		{ Cross(Function); }
		
	BridgeT Cross(void) { return BridgeT(Inner); }
	
	BridgeT operator ->(void) { return std::move(Cross()); }
	
	private:
		InnerT Inner;
};

#endif
