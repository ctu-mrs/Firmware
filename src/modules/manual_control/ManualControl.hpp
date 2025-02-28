/****************************************************************************
 *
 *   Copyright (c) 2021 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#pragma once

#include <drivers/drv_hrt.h>
#include <lib/hysteresis/hysteresis.h>
#include <lib/perf/perf_counter.h>
#include <px4_platform_common/defines.h>
#include <px4_platform_common/module.h>
#include <px4_platform_common/module_params.h>
#include <px4_platform_common/posix.h>
#include <px4_platform_common/px4_work_queue/ScheduledWorkItem.hpp>
#include <uORB/topics/action_request.h>
#include <uORB/topics/landing_gear.h>
#include <uORB/topics/manual_control_switches.h>
#include <uORB/topics/manual_control_setpoint.h>
#include <uORB/topics/parameter_update.h>
#include <uORB/Publication.hpp>
#include <uORB/SubscriptionInterval.hpp>
#include <uORB/SubscriptionCallback.hpp>
#include "ManualControlSelector.hpp"
#include "MovingDiff.hpp"

using namespace time_literals;

class ManualControl : public ModuleBase<ManualControl>, public ModuleParams, public px4::ScheduledWorkItem
{
public:
	ManualControl();
	~ManualControl() override;

	/** @see ModuleBase */
	static int task_spawn(int argc, char *argv[]);

	/** @see ModuleBase */
	static int custom_command(int argc, char *argv[]);

	/** @see ModuleBase */
	static int print_usage(const char *reason = nullptr);

	bool init();

	int print_status() override;

private:
	static constexpr int MAX_MANUAL_INPUT_COUNT = 3;

	void Run() override;
	void processStickArming(const manual_control_setpoint_s &input);

	void evaluateModeSlot(uint8_t mode_slot, uint8_t source);
	void sendActionRequest(int8_t action, int8_t source, int8_t mode = 0);
	void publishLandingGear(int8_t action);

	uORB::Publication<action_request_s> _action_request_pub{ORB_ID(action_request)};
	uORB::Publication<landing_gear_s> _landing_gear_pub{ORB_ID(landing_gear)};

	enum class CameraMode {
		Image = 0,
		Video = 1
	};
	void send_camera_mode_command(CameraMode camera_mode);
	void send_photo_command();
	void send_video_command();

	uORB::Publication<manual_control_setpoint_s> _manual_control_setpoint_pub{ORB_ID(manual_control_setpoint)};

	uORB::SubscriptionInterval _parameter_update_sub{ORB_ID(parameter_update), 1_s};
	int _previous_manual_control_input_instance{-1};
	uORB::SubscriptionCallbackWorkItem _manual_control_setpoint_subs[MAX_MANUAL_INPUT_COUNT] {
		{this, ORB_ID(manual_control_input), 0},
		{this, ORB_ID(manual_control_input), 1},
		{this, ORB_ID(manual_control_input), 2},
	};
	uORB::SubscriptionCallbackWorkItem _manual_control_switches_sub{this, ORB_ID(manual_control_switches)};

	systemlib::Hysteresis _stick_arm_hysteresis{false};
	systemlib::Hysteresis _stick_disarm_hysteresis{false};
	systemlib::Hysteresis _button_hysteresis{false};

	ManualControlSelector _selector;
	bool _published_invalid_once{false};

	MovingDiff _x_diff{};
	MovingDiff _y_diff{};
	MovingDiff _z_diff{};
	MovingDiff _r_diff{};

	manual_control_switches_s _previous_switches{};
	bool _previous_switches_initialized{false};

	hrt_abstime _last_time{0};

	perf_counter_t	_loop_perf{perf_alloc(PC_ELAPSED, MODULE_NAME": cycle")};
	perf_counter_t	_loop_interval_perf{perf_alloc(PC_INTERVAL, MODULE_NAME": interval")};

	DEFINE_PARAMETERS(
		(ParamInt<px4::params::COM_RC_IN_MODE>) _param_com_rc_in_mode,
		(ParamFloat<px4::params::COM_RC_LOSS_T>) _param_com_rc_loss_t,
		(ParamFloat<px4::params::COM_RC_STICK_OV>) _param_com_rc_stick_ov,
		(ParamBool<px4::params::MAN_ARM_GESTURE>) _param_man_arm_gesture,
		(ParamInt<px4::params::COM_RC_ARM_HYST>) _param_com_rc_arm_hyst,
		(ParamBool<px4::params::COM_ARM_SWISBTN>) _param_com_arm_swisbtn,
		(ParamInt<px4::params::COM_FLTMODE1>) _param_fltmode_1,
		(ParamInt<px4::params::COM_FLTMODE2>) _param_fltmode_2,
		(ParamInt<px4::params::COM_FLTMODE3>) _param_fltmode_3,
		(ParamInt<px4::params::COM_FLTMODE4>) _param_fltmode_4,
		(ParamInt<px4::params::COM_FLTMODE5>) _param_fltmode_5,
		(ParamInt<px4::params::COM_FLTMODE6>) _param_fltmode_6,
		(ParamInt<px4::params::MAV_SYS_ID>) _param_mav_sys_id
	)

	unsigned _image_sequence {0};
	bool _video_recording {false}; // TODO: hopefully there is a command soon to toggle without keeping state
};
