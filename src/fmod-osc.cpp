/*==============================================================================
Gain DSP Plugin Example
Copyright (c), Firelight Technologies Pty, Ltd 2004-2014.

This example shows how to create a simple gain DSP effect.
==============================================================================*/

#ifdef WIN32
    #define _CRT_SECURE_NO_WARNINGS
#endif

#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <iostream>
#include <string>

#include "../ip/UdpSocket.h"
#include "../osc/OscOutboundPacketStream.h"

#include "../inc/fmod.hpp"

#define ADDRESS "127.0.0.1"
#define PORT 57120

#define TOP_ROUTE "/fmod"

#define OUTPUT_BUFFER_SIZE 1024

#ifdef WIN32
    #define _CRT_SECURE_NO_WARNINGS
#endif

/*
	First thing the plugin does when it loads is sends a message to supercollider with a random ID
	SC then creates the instruments and sets listeners with this random ID
	Now that version of the plugin can operate independently of the other ones

*/

extern "C" {
    F_DECLSPEC F_DLLEXPORT FMOD_DSP_DESCRIPTION* F_STDCALL FMODGetDSPDescription();
}

const float FMOD_GAIN_PARAM_GAIN_MIN     = -80.0f;
const float FMOD_GAIN_PARAM_GAIN_MAX     = 10.0f;
const float FMOD_GAIN_PARAM_GAIN_DEFAULT = 0.0f;

#define FMOD_GAIN_RAMPCOUNT 256

enum
{

    //FMOD_GAIN_PARAM_INVERT,
	FMOD_GAIN_PARAM_FLOAT1,
	FMOD_GAIN_PARAM_FLOAT2,
	FMOD_GAIN_PARAM_FLOAT3,
	FMOD_GAIN_PARAM_GAIN,
    FMOD_GAIN_NUM_PARAMETERS
};

#define DECIBELS_TO_LINEAR(__dbval__)  ((__dbval__ <= FMOD_GAIN_PARAM_GAIN_MIN) ? 0.0f : powf(10.0f, __dbval__ / 20.0f))
#define LINEAR_TO_DECIBELS(__linval__) ((__linval__ <= 0.0f) ? FMOD_GAIN_PARAM_GAIN_MIN : 20.0f * log10f((float)__linval__))

FMOD_RESULT F_CALLBACK FMOD_Gain_dspcreate       (FMOD_DSP_STATE *dsp_state);
FMOD_RESULT F_CALLBACK FMOD_Gain_dsprelease      (FMOD_DSP_STATE *dsp_state);
FMOD_RESULT F_CALLBACK FMOD_Gain_dspreset        (FMOD_DSP_STATE *dsp_state);
FMOD_RESULT F_CALLBACK FMOD_Gain_dspread         (FMOD_DSP_STATE *dsp_state, float *inbuffer, float *outbuffer, unsigned int length, int inchannels, int *outchannels);
FMOD_RESULT F_CALLBACK FMOD_Gain_dspsetparamfloat(FMOD_DSP_STATE *dsp_state, int index, float value);
FMOD_RESULT F_CALLBACK FMOD_Gain_dspsetparamint  (FMOD_DSP_STATE *dsp_state, int index, int value);
FMOD_RESULT F_CALLBACK FMOD_Gain_dspsetparambool (FMOD_DSP_STATE *dsp_state, int index, bool value);
FMOD_RESULT F_CALLBACK FMOD_Gain_dspsetparamdata (FMOD_DSP_STATE *dsp_state, int index, void *data, unsigned int length);
FMOD_RESULT F_CALLBACK FMOD_Gain_dspgetparamfloat(FMOD_DSP_STATE *dsp_state, int index, float *value, char *valuestr);
FMOD_RESULT F_CALLBACK FMOD_Gain_dspgetparamint  (FMOD_DSP_STATE *dsp_state, int index, int *value, char *valuestr);
FMOD_RESULT F_CALLBACK FMOD_Gain_dspgetparambool (FMOD_DSP_STATE *dsp_state, int index, bool *value, char *valuestr);
FMOD_RESULT F_CALLBACK FMOD_Gain_dspgetparamdata (FMOD_DSP_STATE *dsp_state, int index, void **value, unsigned int *length, char *valuestr);
FMOD_RESULT F_CALLBACK FMOD_Gain_shouldiprocess  (FMOD_DSP_STATE *dsp_state, bool inputsidle, unsigned int length, FMOD_CHANNELMASK inmask, int inchannels, FMOD_SPEAKERMODE speakermode);

static FMOD_DSP_PARAMETER_DESC p_gain;
static FMOD_DSP_PARAMETER_DESC p_invert;

static FMOD_DSP_PARAMETER_DESC p_float1;
static FMOD_DSP_PARAMETER_DESC p_float2;
static FMOD_DSP_PARAMETER_DESC p_float3;
static FMOD_DSP_PARAMETER_DESC p_float4;

FMOD_DSP_PARAMETER_DESC *FMOD_Gain_dspparam[FMOD_GAIN_NUM_PARAMETERS] =
{

    //&p_invert,
	&p_float1,
	&p_float2,
	&p_float3,
	&p_gain
};

FMOD_DSP_DESCRIPTION FMOD_Gain_Desc =
{
    FMOD_PLUGIN_SDK_VERSION,
    "FMOD OSC",   // name
    0x00010000,     // plug-in version
    1,              // number of input buffers to process
    1,              // number of output buffers to process
    FMOD_Gain_dspcreate,
    FMOD_Gain_dsprelease,
    FMOD_Gain_dspreset,
    FMOD_Gain_dspread,
    0,
    0,
    FMOD_GAIN_NUM_PARAMETERS,
    FMOD_Gain_dspparam,
    FMOD_Gain_dspsetparamfloat,
    0, // FMOD_Gain_dspsetparamint,
    0, //FMOD_Gain_dspsetparambool,
    0, // FMOD_Gain_dspsetparamdata,
    FMOD_Gain_dspgetparamfloat,
    0, // FMOD_Gain_dspgetparamint,
    0, //FMOD_Gain_dspgetparambool,
    0, // FMOD_Gain_dspgetparamdata,
    FMOD_Gain_shouldiprocess,
    0
};

extern "C"
{

F_DECLSPEC F_DLLEXPORT FMOD_DSP_DESCRIPTION* F_STDCALL FMODGetDSPDescription()
{
	static float gain_mapping_values[] = { -80, -50, -30, -10, 10 };
	static float gain_mapping_scale[] = { 0, 2, 4, 7, 11 };

    //FMOD_DSP_INIT_PARAMDESC_FLOAT_WITH_MAPPING(p_gain, "Volume", "dB", "Gain in dB. -80 to 10. Default = 0", FMOD_GAIN_PARAM_GAIN_DEFAULT, gain_mapping_values, gain_mapping_scale);
    FMOD_DSP_INIT_PARAMDESC_FLOAT(p_gain, "Volume", "f",  "Sets the gain", 0.0f, 1.0f, 0.0f);
	FMOD_DSP_INIT_PARAMDESC_FLOAT(p_float1, "Wind", "f",  "Adjusts float 1 (Wind)", 0.0f, 1.0f, 0.0f);
	FMOD_DSP_INIT_PARAMDESC_FLOAT(p_float2, "Rain", "f",  "Adjusts float 2 (Rain)", 0.0f, 1.0f, 0.0f);
	FMOD_DSP_INIT_PARAMDESC_FLOAT(p_float3, "Thunder", "f",  "Adjusts float 3 (Thunder)", 0.0f, 1.0f, 0.0f);
	//FMOD_DSP_INIT_PARAMDESC_BOOL(p_invert, "Invert", "", "Invert signal. Default = off", false, 0);
    return &FMOD_Gain_Desc;
}

}

class FMODGainState
{
public:
    FMODGainState();

    void process(float *inbuffer, float *outbuffer, unsigned int length, int channels);
    void reset();
    void setGain(float);
    void setInvert(bool);
	void setFloat1(float);
	void setFloat2(float);
	void setFloat3(float);
	void sendParam(const char *, float);
	void sendMsg(const char *, float);

	void setOSCID(int);

	//void setParams(float);

	float gain() const { return m_current_gain; } //LINEAR_TO_DECIBELS(m_invert ? -m_target_gain : m_target_gain ); }
    bool invert() const { return m_invert; }
	float float1() const { return m_float1; }
	float float2() const { return m_float2; }
	float float3() const { return m_float3; }
	int osc_id() const { return m_osc_id; }

private:
    float m_target_gain;
    float m_current_gain;
    int m_ramp_samples_left;
    bool m_invert;
	float m_float1;
	float m_float2;
	float m_float3;
	float m_params[5];
	int m_osc_id;
	bool was_set;
};

FMODGainState::FMODGainState()
{
    m_target_gain = DECIBELS_TO_LINEAR(FMOD_GAIN_PARAM_GAIN_DEFAULT);
    m_invert = false;

    reset();
}

void FMODGainState::setOSCID(int theID) {

	if (!was_set) {
		m_osc_id = theID;
		sendMsg("/start", m_osc_id);
		was_set = true;
	}
}

void FMODGainState::process(float *inbuffer, float *outbuffer, unsigned int length, int channels)
{
    // Note: buffers are interleaved
   /* float gain = m_current_gain;

    if (m_ramp_samples_left)
    {
        float target = m_target_gain;
        float delta = (target - gain) / m_ramp_samples_left;
        while (length)
        {
            if (--m_ramp_samples_left)
            {
                gain += delta;
                for (int i = 0; i < channels; ++i)
                {
                    *outbuffer++ = *inbuffer++ * gain;
                }
            }
            else
            {
                gain = target;
                break;
            }
            --length;
        }
    }

    unsigned int samples = length * channels;
    while (samples--)
    {
        *outbuffer++ = *inbuffer++ * gain;
    }

    m_current_gain = gain;*/

	//sendParam("/test", 0.5);
}

void FMODGainState::reset()
{
    m_current_gain = m_target_gain;
    m_ramp_samples_left = 0;
}

void FMODGainState::setGain(float gain)
{
    m_target_gain = m_invert ? -DECIBELS_TO_LINEAR(gain) : DECIBELS_TO_LINEAR(gain);
    m_ramp_samples_left = FMOD_GAIN_RAMPCOUNT;
	
	std::string address = TOP_ROUTE;
	address += "/volume";

	UdpTransmitSocket transmitSocket( IpEndpointName( ADDRESS, PORT ) );
    
    char buffer[OUTPUT_BUFFER_SIZE];
    osc::OutboundPacketStream p( buffer, OUTPUT_BUFFER_SIZE );
    
    p << osc::BeginBundleImmediate
        << osc::BeginMessage( address.c_str() )
			<< m_osc_id
            << gain << osc::EndMessage
        << osc::EndBundle;
    
    transmitSocket.Send( p.Data(), p.Size() );

	//m_osc_id = rand() % 100;
}

void FMODGainState::setFloat1(float value) {
	m_float1 = value;
}

void FMODGainState::setFloat2(float value) {
	m_float2 = value;
}

void FMODGainState::setFloat3(float value) {
	m_float3 = value;
}

void FMODGainState::sendParam(const char *addr, float value) {

	std::string address = TOP_ROUTE;
	address += addr;

	UdpTransmitSocket transmitSocket( IpEndpointName( ADDRESS, PORT ) );
    
    char buffer[OUTPUT_BUFFER_SIZE];
    osc::OutboundPacketStream p( buffer, OUTPUT_BUFFER_SIZE );
    
    p << osc::BeginBundleImmediate
        << osc::BeginMessage( address.c_str() ) 
			<< m_osc_id
            << value << osc::EndMessage
        //<< osc::BeginMessage( "/test2" ) 
        //    << true << 24 << (float)10.8 << "world" << osc::EndMessage
        << osc::EndBundle;
    
    transmitSocket.Send( p.Data(), p.Size() );
}

void FMODGainState::sendMsg(const char *addr, float value) {

	std::string address = TOP_ROUTE;
	address += addr;

	UdpTransmitSocket transmitSocket( IpEndpointName( ADDRESS, PORT ) );
    
    char buffer[OUTPUT_BUFFER_SIZE];
    osc::OutboundPacketStream p( buffer, OUTPUT_BUFFER_SIZE );
    
    p << osc::BeginBundleImmediate
        << osc::BeginMessage( address.c_str() )
            << value << osc::EndMessage
        //<< osc::BeginMessage( "/test2" ) 
        //    << true << 24 << (float)10.8 << "world" << osc::EndMessage
        << osc::EndBundle;
    
    transmitSocket.Send( p.Data(), p.Size() );
}


void FMODGainState::setInvert(bool invert)
{
    if (invert != m_invert)
    {
        m_target_gain = -m_target_gain;
        m_ramp_samples_left = FMOD_GAIN_RAMPCOUNT;
    }
    m_invert = invert;
}

FMOD_RESULT F_CALLBACK FMOD_Gain_dspcreate(FMOD_DSP_STATE *dsp_state)
{
    dsp_state->plugindata = (FMODGainState *)FMOD_DSP_STATE_MEMALLOC(dsp_state, sizeof(FMODGainState), FMOD_MEMORY_NORMAL, "FMODGainState");

	FMODGainState *state = (FMODGainState *)dsp_state->plugindata;
	state->setOSCID(rand() % 1000);

    if (!dsp_state->plugindata)
    {
        return FMOD_ERR_MEMORY;
    }
    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK FMOD_Gain_dsprelease(FMOD_DSP_STATE *dsp_state)
{
    FMODGainState *state = (FMODGainState *)dsp_state->plugindata;
	state->sendMsg("/dying", state->osc_id());
    FMOD_DSP_STATE_MEMFREE(dsp_state, state, FMOD_MEMORY_NORMAL, "FMODGainState");
    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK FMOD_Gain_dspread(FMOD_DSP_STATE *dsp_state, float *inbuffer, float *outbuffer, unsigned int length, int inchannels, int * /*outchannels*/)
{
    FMODGainState *state = (FMODGainState *)dsp_state->plugindata;
    state->process(inbuffer, outbuffer, length, inchannels); // input and output channels count match for this effect
    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK FMOD_Gain_dspreset(FMOD_DSP_STATE *dsp_state)
{
    FMODGainState *state = (FMODGainState *)dsp_state->plugindata;
    state->reset();
    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK FMOD_Gain_dspsetparamfloat(FMOD_DSP_STATE *dsp_state, int index, float value)
{
    FMODGainState *state = (FMODGainState *)dsp_state->plugindata;

    switch (index)
    {
    case FMOD_GAIN_PARAM_GAIN:
        state->setGain(value);
        return FMOD_OK;
	case FMOD_GAIN_PARAM_FLOAT1:
		state->setFloat1(value);
		state->sendParam("/float1", value);
		return FMOD_OK;
	case FMOD_GAIN_PARAM_FLOAT2:
		state->setFloat2(value);
		state->sendParam("/float2", value);
		return FMOD_OK;
	case FMOD_GAIN_PARAM_FLOAT3:
		state->setFloat3(value);
		state->sendParam("/float3", value);
		return FMOD_OK;
	}

    return FMOD_ERR_INVALID_PARAM;
}

FMOD_RESULT F_CALLBACK FMOD_Gain_dspgetparamfloat(FMOD_DSP_STATE *dsp_state, int index, float *value, char *valuestr)
{
    FMODGainState *state = (FMODGainState *)dsp_state->plugindata;

    switch (index)
    {
    case FMOD_GAIN_PARAM_GAIN:
        *value = state->gain();
        if (valuestr) sprintf(valuestr, "%.1f dB", state->gain());
        return FMOD_OK;
	case FMOD_GAIN_PARAM_FLOAT1:
		*value = state->float1();
		if (valuestr) sprintf(valuestr, "% fl", state->float1());
		return FMOD_OK;
	case FMOD_GAIN_PARAM_FLOAT2:
		*value = state->float2();
		if (valuestr) sprintf(valuestr, "% fl", state->float2());
		return FMOD_OK;
	case FMOD_GAIN_PARAM_FLOAT3:
		*value = state->float3();
		if (valuestr) sprintf(valuestr, "% fl", state->float3());
		return FMOD_OK;
	}

    return FMOD_ERR_INVALID_PARAM;
}

//FMOD_RESULT F_CALLBACK FMOD_Gain_dspsetparambool(FMOD_DSP_STATE *dsp_state, int index, bool value)
//{
//    FMODGainState *state = (FMODGainState *)dsp_state->plugindata;
//
//    switch (index)
//    {
//      case FMOD_GAIN_PARAM_INVERT:
//        state->setInvert(value);
//        return FMOD_OK;
//    }
//
//    return FMOD_ERR_INVALID_PARAM;
//}

//FMOD_RESULT F_CALLBACK FMOD_Gain_dspgetparambool(FMOD_DSP_STATE *dsp_state, int index, bool *value, char *valuestr)
//{
//    FMODGainState *state = (FMODGainState *)dsp_state->plugindata;
//
//    switch (index)
//    {
//    case FMOD_GAIN_PARAM_INVERT:
//        *value = state->invert();
//        if (valuestr) sprintf(valuestr, state->invert() ? "Inverted" : "Off" );
//        return FMOD_OK;
//    }
//
//    return FMOD_ERR_INVALID_PARAM;
//}

FMOD_RESULT F_CALLBACK FMOD_Gain_shouldiprocess(FMOD_DSP_STATE * /*dsp_state*/, bool inputsidle, unsigned int /*length*/, FMOD_CHANNELMASK /*inmask*/, int /*inchannels*/, FMOD_SPEAKERMODE /*speakermode*/)
{
    if (inputsidle)
    {
        //return FMOD_ERR_DSP_DONTPROCESS;
		return FMOD_OK;
    }

    return FMOD_OK;
}