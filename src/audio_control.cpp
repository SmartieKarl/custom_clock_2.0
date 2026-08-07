#include "audio_control.h"
#include "Arduino.h"
#include "Audio.h"
#include "WiFi.h"
#include "config.h"

AudioTaskControl audioControl; // Global shared instance

namespace
{
enum : uint8_t
{
    SET_VOLUME,
    GET_VOLUME,
    CONNECTTOHOST,
    CONNECTTOSD,
    PAUSE_RESUME,
    STOP_SONG,
};
}

// Implementation
AudioTaskControl::AudioTaskControl()
    : audioSetQueue_(NULL), audioGetQueue_(NULL), taskHandle_(NULL), audio_()
{
}

void AudioTaskControl::createQueues()
{
    audioSetQueue_ = xQueueCreate(10, sizeof(audioMessage));
    audioGetQueue_ = xQueueCreate(10, sizeof(audioMessage));
}

void AudioTaskControl::begin(uint8_t core, UBaseType_t priority)
{
    xTaskCreatePinnedToCore(taskTrampoline, "audioplay", 5000, this,
                            priority | portPRIVILEGE_BIT, &taskHandle_, core);
}

void AudioTaskControl::taskTrampoline(void *arg)
{
    static_cast<AudioTaskControl *>(arg)->taskLoop();
}

void AudioTaskControl::taskLoop()
{
    createQueues();
    if (!audioSetQueue_ || !audioGetQueue_)
    {
        log_e("queues are not initialized");
        while (true)
            ; // endless loop
    }

    audioMessage rxMsg, txMsg;

    audio_.setPinout(Pins::AMP_BCLK, Pins::AMP_LRC, Pins::AMP_DIN);

    audio_.setBufsize(0, 65536); // 64kb/buffer in psram, ~2s playback
    audio_.setVolume(AUDIO_DEFAULT_VOLUME);

    while (true)
    {
        if (xQueueReceive(audioSetQueue_, &rxMsg, 1) == pdPASS)
        {
            if (rxMsg.cmd == 0)
            { // SET_VOLUME
                txMsg.cmd = rxMsg.cmd;
                audio_.setVolume(rxMsg.value);
                txMsg.ret = 1;
                xQueueSend(audioGetQueue_, &txMsg, portMAX_DELAY);
            }
            else if (rxMsg.cmd == 2)
            { // CONNECTTOHOST
                txMsg.cmd = rxMsg.cmd;
                txMsg.ret = audio_.connecttohost(rxMsg.txt);
                xQueueSend(audioGetQueue_, &txMsg, portMAX_DELAY);
            }
            else if (rxMsg.cmd == 3)
            { // CONNECTTOSD
                txMsg.cmd = rxMsg.cmd;
                txMsg.ret = audio_.connecttoSD(rxMsg.txt);
                xQueueSend(audioGetQueue_, &txMsg, portMAX_DELAY);
            }
            else if (rxMsg.cmd == 1)
            { // GET_VOLUME
                txMsg.cmd = rxMsg.cmd;
                txMsg.ret = audio_.getVolume();
                xQueueSend(audioGetQueue_, &txMsg, portMAX_DELAY);
            }
            else if (rxMsg.cmd == PAUSE_RESUME)
            {
                txMsg.cmd = rxMsg.cmd;
                txMsg.ret = audio_.pauseResume();
                xQueueSend(audioGetQueue_, &txMsg, portMAX_DELAY);
            }
            else if (rxMsg.cmd == STOP_SONG)
            {
                txMsg.cmd = rxMsg.cmd;
                txMsg.ret = audio_.stopSong();
                xQueueSend(audioGetQueue_, &txMsg, portMAX_DELAY);
            }
            else
            {
                log_i("error");
            }
        }
        audio_.loop();
    }
}

AudioTaskControl::audioMessage AudioTaskControl::transmitReceive(audioMessage msg)
{
    audioMessage recv;
    xQueueSend(audioSetQueue_, &msg, portMAX_DELAY);
    if (xQueueReceive(audioGetQueue_, &recv, portMAX_DELAY) == pdPASS)
    {
        if (msg.cmd != recv.cmd)
        {
            log_e("wrong reply from message queue");
        }
    }
    return recv;
}

void AudioTaskControl::setVolume(uint8_t vol)
{
    audioMessage m{};
    m.cmd = 0; // SET_VOLUME
    m.value = vol;
    transmitReceive(m);
}

uint8_t AudioTaskControl::getVolume()
{
    audioMessage m{};
    m.cmd = 1; // GET_VOLUME
    audioMessage r = transmitReceive(m);
    return static_cast<uint8_t>(r.ret);
}

bool AudioTaskControl::connectToHost(const char *host)
{
    audioMessage m{};
    m.cmd = 2; // CONNECTTOHOST
    m.txt = host;
    audioMessage r = transmitReceive(m);
    return r.ret;
}

bool AudioTaskControl::connectToSD(const char *filename)
{
    audioMessage m{};
    m.cmd = 3; // CONNECTTOSD
    m.txt = filename;
    audioMessage r = transmitReceive(m);
    return r.ret;
}

bool AudioTaskControl::pause()
{
    audioMessage m{};
    m.cmd = 4; // PAUSE_RESUME
    audioMessage r = transmitReceive(m);
    return r.ret;
}

uint32_t AudioTaskControl::stop()
{
    audioMessage m{};
    m.cmd = 5; // STOP_SONG
    audioMessage r = transmitReceive(m);
    return r.ret;
}