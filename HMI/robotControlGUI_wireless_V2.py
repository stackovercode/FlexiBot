import sys
import time
import threading
from kivy.app import App
from kivy.clock import Clock
from kivy.core.window import Window
from kivy.uix.screenmanager import ScreenManager, Screen
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.label import Label
from kivy.uix.button import Button
from kivy.uix.togglebutton import ToggleButton
from kivy.uix.textinput import TextInput
from kivy.uix.slider import Slider
from kivy.properties import StringProperty

try:
    from kivymd.app import MDApp
    from kivymd.uix.button import MDRaisedButton, MDFlatButton
except ImportError:
    MDApp = App
    MDRaisedButton = Button
    MDFlatButton = Button
import requests
import serial

Window.clearcolor = (1, 1, 1, 1)  # White background

# -----------------------------
# Serial
# -----------------------------
#DEFAULT_SERIAL_PORT = '/dev/cu.usbmodem2101'
DEFAULT_SERIAL_PORT = '/dev/cu.usbmodem211401'
DEFAULT_BAUD_RATE = 115200

try:
    ser = serial.Serial(DEFAULT_SERIAL_PORT, DEFAULT_BAUD_RATE, timeout=1)
    time.sleep(2)
    print(f"Connected to {DEFAULT_SERIAL_PORT} at {DEFAULT_BAUD_RATE} baud.")
except serial.SerialException as e:
    print(f"Error opening serial port {DEFAULT_SERIAL_PORT}: {e}")
    ser = None

# --------------------------------------------------------------------
class RobotBackend:
    def __init__(self, use_wireless=False):
        self.use_wireless = use_wireless
        self.ip_address = "192.168.3.1"  # default IP
        self.status_callback = None

    def set_status_callback(self, callback):
        """So we can push messages to a UI label from the concurrency thread."""
        self.status_callback = callback

    def send_command(self, command: str):
        print(f"[RobotBackend] send_command: {command}")
        if self.use_wireless:
            self._send_web_command(command)
        else:
            self._send_serial_command(command)

    def _send_web_command(self, command: str):
        def _threaded_send():
            try:
                url = f"http://{self.ip_address}:80/{command}"
                print(f"[HTTP] GET -> {url}")
                response = requests.get(url, timeout=5)
                if response.status_code == 200:
                    self.update_status("Command Sent Successfully (HTTP)")
                else:
                    self.update_status(f"Error: HTTP {response.status_code}")
            except requests.exceptions.Timeout:
                self.update_status("Error: HTTP Request Timed Out")
            except requests.exceptions.ConnectionError:
                self.update_status("Error: Connection Failed")
            except Exception as e:
                self.update_status(f"Error: {e}")

        threading.Thread(target=_threaded_send, daemon=True).start()

    def _send_serial_command(self, command: str):
        if ser and ser.is_open:
            full_cmd = command + "\n"
            ser.write(full_cmd.encode('utf-8'))
            print(f"Sent (Serial): {command.strip()}")
        else:
            print("Serial port not connected.")

    def update_status(self, message):
        print(f"[RobotBackend] update_status -> {message}")
        if self.status_callback:
            self.status_callback(message)

    def read_serial_thread(self):
        print("[RobotBackend] Serial reading thread started")
        while True:
            try:
                if ser and ser.in_waiting:
                    line_bytes = ser.readline()
                    line_str = line_bytes.decode('utf-8').strip()
                    print(f"Received from serial: {line_str}")
                    if line_str.startswith("STATUS:"):
                        msg = line_str.replace("STATUS:", "").strip()
                        self.update_status(msg)
            except serial.SerialException:
                self.update_status("Error: Serial connection lost.")
                print("Serial connection lost.")
                break
            except UnicodeDecodeError:
                print("UnicodeDecodeError in reading serial.")
                continue
            time.sleep(0.01)

# --------------------------------------------------------------------
class StatusLabel(Label):
    status_text = StringProperty("Waiting for status...")

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.color = (0, 0, 0, 1)  # black text for white background
        self.bind(status_text=self.on_status_text)

    def on_status_text(self, instance, value):
        self.text = value


# ==========================
# Screen: Main Menu
# ==========================
class MainMenuScreen(Screen):
    """The home screen: big buttons to go to limb, body, calibration/gait."""
    def __init__(self, backend: RobotBackend, **kwargs):
        super().__init__(**kwargs)
        self.backend = backend

        layout = BoxLayout(orientation='vertical', spacing=20, padding=20)

        title_label = Label(text="Main Menu", font_size='54sp', bold=True, color=(0,0,0,1))
        layout.add_widget(title_label)

        btn_limb = Button(text="Limb Control", background_color=(0.5, 0.8, 1, 1), color=(0,0,0,1), size_hint=(1,0.15), font_size='30sp')
        btn_body = Button(text="Body Control", background_color=(0.5, 0.8, 1, 1), color=(0,0,0,1), size_hint=(1,0.15), font_size='30sp')
        btn_calib = Button(text="Calibration & Gait", background_color=(0.5, 0.8, 1, 1), color=(0,0,0,1), size_hint=(1,0.15), font_size='30sp')

        btn_limb.bind(on_press=self.goto_limb)
        btn_body.bind(on_press=self.goto_body)
        btn_calib.bind(on_press=self.goto_calib)

        layout.add_widget(btn_limb)
        layout.add_widget(btn_body)
        layout.add_widget(btn_calib)

        self.add_widget(layout)

    def goto_limb(self, instance):
        self.manager.current = "limb_screen"

    def goto_body(self, instance):
        self.manager.current = "body_screen"

    def goto_calib(self, instance):
        self.manager.current = "calib_screen"


# ==========================
# Screen: Limb Control
# ==========================
class LimbControlScreen(Screen):
    """
    Controls for Limb1..4 (M1..M8).
    Added:
      - A TextInput for "Duration" (ms)
      - A Slider for "Pulse" (µs, 500..2500)
    So each button can read these two values before sending the command.
    
    NOTE: On your Arduino side, you might need to parse "pulse_duration" as "1500_2000"
    and split by underscore. Right now, your code might interpret everything after ':'
    as one integer (duration). 
    """
    def __init__(self, backend: RobotBackend, **kwargs):
        super().__init__(**kwargs)
        self.backend = backend

        main_layout = BoxLayout(orientation='vertical', spacing=10, padding=10)

        title = Label(text="Limb Control Screen", font_size='48sp', color=(0,0,0,1))
        main_layout.add_widget(title)

        config_box = BoxLayout(orientation='horizontal', spacing=10, size_hint=(1, 0.15))

        self.duration_input = TextInput(
            text='500',
            multiline=False,
            size_hint=(0.3, 1),
            hint_text="Duration (ms)"
        )
        config_box.add_widget(self.duration_input)


        self.pulse_slider = Slider(min=500, max=2500, value=1500, step=50, size_hint=(0.7, 1))
        config_box.add_widget(self.pulse_slider)

        main_layout.add_widget(config_box)

        def getPulseDuration():
            dur_str = self.duration_input.text.strip()
            if not dur_str.isdigit():
                dur_str = "500"
            pulse_val = int(self.pulse_slider.value)
            return f"{pulse_val}_{dur_str}"  # e.g. "1500_2000"


        limb1_box = BoxLayout(orientation='horizontal', spacing=5, size_hint=(1,0.15))

        btn_l1_m1_cw = Button(text="L1 M1 CW", color=(0,0,0,1),font_size='24sp')
        btn_l1_m2_cw = Button(text="L1 M2 CW", color=(0,0,0,1),font_size='24sp')
        btn_l1_m1_ccw = Button(text="L1 M1 CCW", color=(0,0,0,1),font_size='24sp')
        btn_l1_m2_ccw = Button(text="L1 M2 CCW", color=(0,0,0,1),font_size='24sp')
        btn_l1_stop = Button(text="Stop L1", background_color=(1,0,0,1), color=(1,1,1,1),font_size='24sp')

        ##############################
        # Limb1 => M1 & M2
        ##############################
        limb1_box = BoxLayout(orientation='horizontal', spacing=5, size_hint=(1,0.15))

        btn_l1_m1_cw = Button(text="L1 M1 CW", color=(0,0,0,1),font_size='24sp')
        btn_l1_m1_ccw = Button(text="L1 M1 CCW", color=(0,0,0,1),font_size='24sp')
        btn_l1_m2_cw = Button(text="L1 M2 CW", color=(0,0,0,1),font_size='24sp')
        btn_l1_m2_ccw = Button(text="L1 M2 CCW", color=(0,0,0,1),font_size='24sp')
        btn_l1_stop = Button(text="Stop L1", background_color=(1,0,0,1), color=(1,1,1,1),font_size='24sp')

        btn_l1_m1_cw.bind(
            on_press=lambda x: self.backend.send_command(f"ROTATE_M1_CW:{getPulseDuration()}")
        )
        btn_l1_m1_ccw.bind(
            on_press=lambda x: self.backend.send_command(f"ROTATE_M1_CCW:{getPulseDuration()}")
        )
        btn_l1_m2_cw.bind(
            on_press=lambda x: self.backend.send_command(f"ROTATE_M2_CW:{getPulseDuration()}")
        )
        btn_l1_m2_ccw.bind(
            on_press=lambda x: self.backend.send_command(f"ROTATE_M2_CCW:{getPulseDuration()}")
        )
        btn_l1_stop.bind(
            on_press=lambda x: self.backend.send_command("STOP_M1_M2_MOTORS")
        )

        limb1_box.add_widget(btn_l1_m1_cw)
        limb1_box.add_widget(btn_l1_m1_ccw)
        limb1_box.add_widget(btn_l1_m2_cw)
        limb1_box.add_widget(btn_l1_m2_ccw)
        limb1_box.add_widget(btn_l1_stop)
        main_layout.add_widget(limb1_box)

        ##############################
        # Limb2 => M3 & M4
        ##############################
        limb2_box = BoxLayout(orientation='horizontal', spacing=5, size_hint=(1,0.15))

        btn_l2_m3_cw = Button(text="L2 M3 CW", color=(0,0,0,1), font_size='24sp')
        btn_l2_m3_ccw = Button(text="L2 M3 CCW", color=(0,0,0,1), font_size='24sp')
        btn_l2_m4_cw = Button(text="L2 M4 CW", color=(0,0,0,1), font_size='24sp')
        btn_l2_m4_ccw = Button(text="L2 M4 CCW", color=(0,0,0,1), font_size='24sp')
        btn_l2_stop = Button(text="Stop L2", background_color=(1,0,0,1), color=(1,1,1,1), font_size='24sp')

        btn_l2_m3_cw.bind(
            on_press=lambda x: self.backend.send_command(f"ROTATE_M3_CW:{getPulseDuration()}")
        )
        btn_l2_m3_ccw.bind(
            on_press=lambda x: self.backend.send_command(f"ROTATE_M3_CCW:{getPulseDuration()}")
        )
        btn_l2_m4_cw.bind(
            on_press=lambda x: self.backend.send_command(f"ROTATE_M4_CW:{getPulseDuration()}")
        )
        btn_l2_m4_ccw.bind(
            on_press=lambda x: self.backend.send_command(f"ROTATE_M4_CCW:{getPulseDuration()}")
        )
        btn_l2_stop.bind(
            on_press=lambda x: self.backend.send_command("STOP_M3_M4_MOTORS")
        )

        limb2_box.add_widget(btn_l2_m3_cw)
        limb2_box.add_widget(btn_l2_m3_ccw)
        limb2_box.add_widget(btn_l2_m4_cw)
        limb2_box.add_widget(btn_l2_m4_ccw)
        limb2_box.add_widget(btn_l2_stop)
        main_layout.add_widget(limb2_box)

        ##############################
        # Limb3 => M5 & M6
        ##############################
        limb3_box = BoxLayout(orientation='horizontal', spacing=5, size_hint=(1,0.15))

        btn_l3_m5_cw = Button(text="L3 M5 CW", color=(0,0,0,1), font_size='24sp')
        btn_l3_m5_ccw = Button(text="L3 M5 CCW", color=(0,0,0,1), font_size='24sp')
        btn_l3_m6_cw = Button(text="L3 M6 CW", color=(0,0,0,1), font_size='24sp')
        btn_l3_m6_ccw = Button(text="L3 M6 CCW", color=(0,0,0,1), font_size='24sp')
        btn_l3_stop = Button(text="Stop L3", background_color=(1,0,0,1), color=(1,1,1,1), font_size='24sp')

        btn_l3_m5_cw.bind(
            on_press=lambda x: self.backend.send_command(f"ROTATE_M5_CW:{getPulseDuration()}")
        )
        btn_l3_m5_ccw.bind(
            on_press=lambda x: self.backend.send_command(f"ROTATE_M5_CCW:{getPulseDuration()}")
        )
        btn_l3_m6_cw.bind(
            on_press=lambda x: self.backend.send_command(f"ROTATE_M6_CW:{getPulseDuration()}")
        )
        btn_l3_m6_ccw.bind(
            on_press=lambda x: self.backend.send_command(f"ROTATE_M6_CCW:{getPulseDuration()}")
        )
        btn_l3_stop.bind(
            on_press=lambda x: self.backend.send_command("STOP_M5_M6_MOTORS")
        )

        limb3_box.add_widget(btn_l3_m5_cw)
        limb3_box.add_widget(btn_l3_m5_ccw)
        limb3_box.add_widget(btn_l3_m6_cw)
        limb3_box.add_widget(btn_l3_m6_ccw)
        limb3_box.add_widget(btn_l3_stop)
        main_layout.add_widget(limb3_box)

        ##############################
        # Limb4 => M7 & M8
        ##############################
        limb4_box = BoxLayout(orientation='horizontal', spacing=5, size_hint=(1,0.15))

        btn_l4_m7_cw = Button(text="L4 M7 CW", color=(0,0,0,1), font_size='24sp')
        btn_l4_m7_ccw = Button(text="L4 M7 CCW", color=(0,0,0,1), font_size='24sp')
        btn_l4_m8_cw = Button(text="L4 M8 CW", color=(0,0,0,1), font_size='24sp')
        btn_l4_m8_ccw = Button(text="L4 M8 CCW", color=(0,0,0,1), font_size='24sp')
        btn_l4_stop = Button(text="Stop L4", background_color=(1,0,0,1), color=(1,1,1,1), font_size='24sp')

        btn_l4_m7_cw.bind(
            on_press=lambda x: self.backend.send_command(f"ROTATE_M7_CW:{getPulseDuration()}")
        )
        btn_l4_m7_ccw.bind(
            on_press=lambda x: self.backend.send_command(f"ROTATE_M7_CCW:{getPulseDuration()}")
        )
        btn_l4_m8_cw.bind(
            on_press=lambda x: self.backend.send_command(f"ROTATE_M8_CW:{getPulseDuration()}")
        )
        btn_l4_m8_ccw.bind(
            on_press=lambda x: self.backend.send_command(f"ROTATE_M8_CCW:{getPulseDuration()}")
        )
        btn_l4_stop.bind(
            on_press=lambda x: self.backend.send_command("STOP_M7_M8_MOTORS")
        )

        limb4_box.add_widget(btn_l4_m7_cw)
        limb4_box.add_widget(btn_l4_m7_ccw)
        limb4_box.add_widget(btn_l4_m8_cw)
        limb4_box.add_widget(btn_l4_m8_ccw)
        limb4_box.add_widget(btn_l4_stop)
        main_layout.add_widget(limb4_box)

        btn_back = Button(text="<< Back to Main Menu", size_hint=(1,0.15),
                          background_color=(0.6,0.6,0.6,1), color=(0,0,0,1), font_size='24sp')
        btn_back.bind(on_press=lambda x: setattr(self.manager, 'current', 'main_menu'))
        main_layout.add_widget(btn_back)

        self.add_widget(main_layout)


# ==========================
# Screen: Body Control
# ==========================
class BodyControlScreen(Screen):
    """
    Controls for M9..M10 (body motors), plus stand/sit, etc.
    """
    def __init__(self, backend: RobotBackend, **kwargs):
        super().__init__(**kwargs)
        self.backend = backend

        layout = BoxLayout(orientation='vertical', spacing=10, padding=10)

        title = Label(text="Body Control Screen", font_size='48sp', color=(0,0,0,1))
        layout.add_widget(title)

        # Body Motor 1 => M9
        box_m9 = BoxLayout(orientation='horizontal', spacing=5, size_hint=(1,0.15))
        btn_m9_cw = Button(text="Body1 CW", color=(0,0,0,1), font_size='24sp')
        btn_m9_ccw = Button(text="Body1 CCW", color=(0,0,0,1), font_size='24sp')
        btn_m9_stop = Button(text="Stop Body1", background_color=(1,0,0,1), color=(1,1,1,1), font_size='24sp')

        btn_m9_cw.bind(on_press=lambda x: self.backend.send_command("ROTATE_BODY1_CW:500"))
        btn_m9_ccw.bind(on_press=lambda x: self.backend.send_command("ROTATE_BODY1_CCW:500"))
        btn_m9_stop.bind(on_press=lambda x: self.backend.send_command("STOP_BODY1"))

        box_m9.add_widget(btn_m9_cw)
        box_m9.add_widget(btn_m9_ccw)
        box_m9.add_widget(btn_m9_stop)
        layout.add_widget(box_m9)

        # Body Motor 2 => M10
        box_m10 = BoxLayout(orientation='horizontal', spacing=5, size_hint=(1,0.15))
        btn_m10_cw = Button(text="Body2 CW", color=(0,0,0,1), font_size='24sp')
        btn_m10_ccw = Button(text="Body2 CCW", color=(0,0,0,1), font_size='24sp')
        btn_m10_stop = Button(text="Stop Body2", background_color=(1,0,0,1), color=(1,1,1,1), font_size='24sp')

        btn_m10_cw.bind(on_press=lambda x: self.backend.send_command("ROTATE_BODY2_CW:500"))
        btn_m10_ccw.bind(on_press=lambda x: self.backend.send_command("ROTATE_BODY2_CCW:500"))
        btn_m10_stop.bind(on_press=lambda x: self.backend.send_command("STOP_BODY2"))

        box_m10.add_widget(btn_m10_cw)
        box_m10.add_widget(btn_m10_ccw)
        box_m10.add_widget(btn_m10_stop)
        layout.add_widget(box_m10)

        # Stand Up / Sit Down
        box_stand = BoxLayout(orientation='horizontal', spacing=5, size_hint=(1,0.15))
        btn_stand = Button(text="STAND_UP", background_color=(0,1,0,1), color=(0,0,0,1), font_size='24sp')
        btn_sit = Button(text="SIT_DOWN", background_color=(0,1,0,1), color=(0,0,0,1), font_size='24sp')

        btn_stand.bind(on_press=lambda x: self.backend.send_command("STAND_UP"))
        btn_sit.bind(on_press=lambda x: self.backend.send_command("SIT_DOWN"))

        box_stand.add_widget(btn_stand)
        box_stand.add_widget(btn_sit)
        layout.add_widget(box_stand)

        btn_back = Button(text="<< Back to Main Menu", size_hint=(1,0.15),
                          background_color=(0.6,0.6,0.6,1), color=(0,0,0,1), font_size='24sp')
        btn_back.bind(on_press=lambda x: setattr(self.manager, 'current', 'main_menu'))
        layout.add_widget(btn_back)

        self.add_widget(layout)


# ==========================
# Screen: Calibration & Gait
# ==========================
class CalibGaitScreen(Screen):
    """
    Contains calibration, crawling/walking/fast crawl, speed slider, status label.
    """

    def __init__(self, backend: RobotBackend, **kwargs):
        super().__init__(**kwargs)
        self.backend = backend

        main_layout = BoxLayout(orientation='vertical', spacing=10, padding=10)

        lbl_title = Label(text="Calibration & Gait", font_size='48sp', color=(0,0,0,1))
        main_layout.add_widget(lbl_title)

        cal_box = BoxLayout(orientation='horizontal', spacing=5, size_hint=(1,0.15))
        btn_cal_limb1 = Button(text="Calibrate L1", background_color=(0,0.5,0,1), color=(1,1,1,1), font_size='24sp')
        btn_cal_limb2 = Button(text="Calibrate L2", background_color=(0,0.5,0,1), color=(1,1,1,1), font_size='24sp')
        btn_cal_limb3 = Button(text="Calibrate L3", background_color=(0,0.5,0,1), color=(1,1,1,1), font_size='24sp')
        btn_cal_limb4 = Button(text="Calibrate L4", background_color=(0,0.5,0,1), color=(1,1,1,1), font_size='24sp')
        btn_cal_all = Button(text="Calibrate All", background_color=(0,0.3,0,1), color=(1,1,1,1), font_size='24sp')

        btn_cal_limb1.bind(on_press=lambda x: self.backend.send_command("CALIBRATE_LIMB:1"))
        btn_cal_limb2.bind(on_press=lambda x: self.backend.send_command("CALIBRATE_LIMB:2"))
        btn_cal_limb3.bind(on_press=lambda x: self.backend.send_command("CALIBRATE_LIMB:3"))
        btn_cal_limb4.bind(on_press=lambda x: self.backend.send_command("CALIBRATE_LIMB:4"))
        btn_cal_all.bind(on_press=lambda x: self.backend.send_command("CALIBRATE_ALL_LIMBS"))

        cal_box.add_widget(btn_cal_limb1)
        cal_box.add_widget(btn_cal_limb2)
        cal_box.add_widget(btn_cal_limb3)
        cal_box.add_widget(btn_cal_limb4)
        cal_box.add_widget(btn_cal_all)
        main_layout.add_widget(cal_box)

        # Gait Row
        gait_box = BoxLayout(orientation='horizontal', spacing=5, size_hint=(1,0.15))
        btn_crawl = Button(text="CRAWLING", background_color=(1,0.84,0,1), color=(0,0,0,1), font_size='24sp')
        btn_walk = Button(text="WALKING", background_color=(1,0.84,0,1), color=(0,0,0,1), font_size='24sp')
        btn_fast = Button(text="FASTCRAWL", background_color=(1,0.84,0,1), color=(0,0,0,1), font_size='24sp')
        btn_stop = Button(text="STOP_GAIT", background_color=(1,0,0,1), color=(1,1,1,1), font_size='24sp')

        btn_crawl.bind(on_press=lambda x: self.backend.send_command("START_CRAWLING"))
        btn_walk.bind(on_press=lambda x: self.backend.send_command("START_WALKING"))
        btn_fast.bind(on_press=lambda x: self.backend.send_command("START_FASTCRAWL"))
        btn_stop.bind(on_press=lambda x: self.backend.send_command("STOP_GAIT"))

        gait_box.add_widget(btn_crawl)
        gait_box.add_widget(btn_walk)
        gait_box.add_widget(btn_fast)
        gait_box.add_widget(btn_stop)
        main_layout.add_widget(gait_box)

        # Speed Slider
        slider_box = BoxLayout(orientation='horizontal', spacing=5, size_hint=(1,0.15))
        lbl_speed = Label(text="Motor Speed:", color=(0,0,0,1), font_size='24sp', size_hint=(0.2,1))
        self.speed_slider = Slider(min=0, max=255, value=128, size_hint=(0.6,1))
        self.speed_slider.bind(value=self.on_speed_slider)

        slider_box.add_widget(lbl_speed)
        slider_box.add_widget(self.speed_slider)
        main_layout.add_widget(slider_box)

        self.status_label = StatusLabel(size_hint=(1,0.15))
        main_layout.add_widget(self.status_label)
        self.backend.set_status_callback(self.update_status_label)

        btn_back = Button(text="<< Back to Main Menu", size_hint=(1,0.15),
                          background_color=(0.6,0.6,0.6,1), color=(0,0,0,1), font_size='24sp')
        btn_back.bind(on_press=lambda x: setattr(self.manager, 'current', 'main_menu'))
        main_layout.add_widget(btn_back)

        self.add_widget(main_layout)

    def on_enter(self, *args):
            print("[CalibGaitScreen] on_enter -> sending SET_MODE:GAIT")
            self.backend.send_command("SET_MODE:GAIT")
            return super().on_enter(*args)

    def on_leave(self, *args):
        print("[CalibGaitScreen] on_leave -> sending SET_MODE:INDIVIDUAL (optional)")
        self.backend.send_command("SET_MODE:INDIVIDUAL")
        return super().on_leave(*args)

    def on_speed_slider(self, instance, value):
        print(f"[CalibGaitScreen] Speed slider => {value}")
        cmd = f"SET_SPEED:{int(value)}"
        self.backend.send_command(cmd)

    def update_status_label(self, message):
        self.status_label.status_text = message


# ==========================
# The main App
# ==========================
class MultiWindowRobotApp(App):
    def __init__(self, use_wireless=False, **kwargs):
        super().__init__(**kwargs)
        self.use_wireless = use_wireless
        self.backend = RobotBackend(use_wireless=self.use_wireless)

    def build(self):
        self.title = "Robot Control HMI"

        sm = ScreenManager()

        main_menu = MainMenuScreen(self.backend, name='main_menu')
        limb_screen = LimbControlScreen(self.backend, name='limb_screen')
        body_screen = BodyControlScreen(self.backend, name='body_screen')
        calib_screen = CalibGaitScreen(self.backend, name='calib_screen')

        sm.add_widget(main_menu)
        sm.add_widget(limb_screen)
        sm.add_widget(body_screen)
        sm.add_widget(calib_screen)

        threading.Thread(target=self.backend.read_serial_thread, daemon=True).start()

        return sm

    def on_stop(self):
        if ser and ser.is_open:
            ser.close()
            print("Serial connection closed.")
        print("Application stopped.")


# -----------------------------
# Run the Application
# -----------------------------
if __name__ == '__main__':
    choice = input("Use wireless? (y/n): ").strip().lower()
    use_wireless = (choice.startswith('y'))

    MultiWindowRobotApp(use_wireless=use_wireless).run()
