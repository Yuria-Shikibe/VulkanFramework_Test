//
// Created by Matrix on 2024/6/10.
//

export module Core.ApplicationTimer;

export import Core.Unit;
import std;

export namespace Core{
	template <typename T>
		requires (std::is_floating_point_v<T>)
    // using T = float;
	class ApplicationTimer{
		using Tick = DirectAccessTimeUnit<T, TickRatio>;
		using Sec = DirectAccessTimeUnit<T>;

	    Sec globalTime{};
	    Sec globalDelta{};
	    Sec updateTime{};
	    Sec updateDelta{};

		DeltaSetter deltaSetter{nullptr};
		TimerSetter timerSetter{nullptr};
	    TimeReseter timeReseter{nullptr};

		bool paused = false;
	    //TODO time scale?

	public:
		static constexpr T TicksPerSecond{Tick::period::den};

        [[nodiscard]] constexpr ApplicationTimer() noexcept = default;

        [[nodiscard]] constexpr ApplicationTimer(const DeltaSetter deltaSetter, const TimerSetter timerSetter, const TimeReseter timeReseter) noexcept :
            deltaSetter{deltaSetter},
            timerSetter{timerSetter},
            timeReseter{timeReseter} {}

        constexpr void pause() noexcept {paused = true;}

		[[nodiscard]] constexpr bool isPaused() const noexcept{return paused;}

		constexpr void resume() noexcept {paused = false;}

		//TODO fire an event?
		constexpr void setPause(const bool v) noexcept {paused = v;}
		
		[[nodiscard]] constexpr DeltaSetter getDeltaSetter() const noexcept{ return deltaSetter; }

		constexpr void setDeltaSetter(const DeltaSetter deltaSetter) noexcept{ this->deltaSetter = deltaSetter; }

		[[nodiscard]] constexpr TimerSetter getTimerSetter() const noexcept{ return timerSetter; }

		constexpr void setTimerSetter(const TimerSetter timerSetter) noexcept{ this->timerSetter = timerSetter; }

		void fetchTime(){
#if DEBUG_CHECK
			if(!timerSetter || !deltaSetter){
				throw std::invalid_argument{"Missing Timer Function Pointer"};
			}
#endif

			globalDelta = deltaSetter(globalTime);
			globalTime = timerSetter();

			updateDelta = paused ? Sec{0} : globalDelta;
			updateTime += updateDelta;
		}

		void resetTime() {
#if DEBUG_CHECK
		    if(!timeReseter){
		        throw std::invalid_argument{"Missing Timer Function Pointer"};
		    }
#endif

			updateTime = globalTime = 0;
		    timeReseter(0);
		}

		[[nodiscard]] constexpr Sec getGlobalDelta() const noexcept{return {globalDelta};}
		[[nodiscard]] constexpr Sec getUpdateDelta() const noexcept{return {updateDelta};}

		[[nodiscard]] constexpr Sec getGlobalTime() const noexcept{return {globalTime};}
		[[nodiscard]] constexpr Sec getUpdateTime() const noexcept{return {updateTime};}

		[[nodiscard]] constexpr Tick globalDeltaTick() const noexcept{return getGlobalDelta();}
		[[nodiscard]] constexpr Tick updateDeltaTick() const noexcept{return getGlobalDelta();}

	};
	

	
}
