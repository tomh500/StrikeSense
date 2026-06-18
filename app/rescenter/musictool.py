import sys
import os
import subprocess
from PyQt6.QtWidgets import (QApplication, QWidget, QLabel, QPushButton, 
                             QLineEdit, QFileDialog, QComboBox, QSlider, 
                             QCheckBox, QVBoxLayout, QHBoxLayout, QMessageBox, QGridLayout)
from PyQt6.QtCore import Qt, QUrl
from PyQt6.QtMultimedia import QMediaPlayer, QAudioOutput

class AudioConverterApp(QWidget):
    def __init__(self):
        super().__init__()
        self.initUI()
        
        # 初始化播放器（用于试听）
        self.player = QMediaPlayer()
        self.audio_output = QAudioOutput()
        self.player.setAudioOutput(self.audio_output)
        self.temp_preview_path = "temp_preview.mp3"

    def initUI(self):
        self.setWindowTitle('FFmpeg 音频剪辑与转换工具')
        self.resize(500, 350)

        # 布局管理
        layout = QVBoxLayout()
        grid = QGridLayout()

        # 1. 文件选择
        self.btn_select = QPushButton('选择文件 (mp4/mp3/wav/ogg/m4a...)')
        self.btn_select.clicked.connect(self.select_file)
        self.lbl_file = QLabel('未选择文件')
        self.lbl_file.setWordWrap(True)

        # 2. 截取时间
        self.lbl_start = QLabel('开始时间 (秒 或 hh:mm:ss):')
        self.txt_start = QLineEdit('0')
        self.lbl_end = QLabel('结束时间 (秒 或 hh:mm:ss):')
        self.txt_end = QLineEdit('')
        self.txt_end.setPlaceholderText('留空至文件结束')

        # 3. 音量调整
        self.lbl_volume = QLabel('音量调整 (100%):')
        self.slider_volume = QSlider(Qt.Orientation.Horizontal)
        self.slider_volume.setMinimum(0)
        self.slider_volume.setMaximum(200)
        self.slider_volume.setValue(100)
        self.slider_volume.valueChanged.connect(self.update_volume_label)

        # 4. 导出格式与压缩
        self.lbl_format = QLabel('导出格式:')
        self.combo_format = QComboBox()
        self.combo_format.addItems(['mp3', 'wav', 'ogg', 'm4a', 'flac'])
        self.combo_format.currentTextChanged.connect(self.toggle_compression_box)
        
        self.cb_compress = QCheckBox('启用压缩/低码率 (针对无损格式节约体积)')
        self.cb_compress.setEnabled(False) # 默认mp3不需要这个勾选

        # 5. 试听与导出按钮
        self.btn_preview = QPushButton('试听截取片段')
        self.btn_preview.clicked.connect(self.preview_audio)
        self.btn_preview.setStyleSheet("background-color: #4CAF50; color: white;")

        self.btn_export = QPushButton('开始导出')
        self.btn_export.clicked.connect(self.export_audio)
        self.btn_export.setStyleSheet("background-color: #008CBA; color: white;")

        # 将组件添加到网格
        grid.addWidget(self.lbl_start, 0, 0)
        grid.addWidget(self.txt_start, 0, 1)
        grid.addWidget(self.lbl_end, 1, 0)
        grid.addWidget(self.txt_end, 1, 1)
        grid.addWidget(self.lbl_volume, 2, 0)
        grid.addWidget(self.slider_volume, 2, 1)
        grid.addWidget(self.lbl_format, 3, 0)
        grid.addWidget(self.combo_format, 3, 1)

        # 组合整体布局
        layout.addWidget(self.btn_select)
        layout.addWidget(self.lbl_file)
        layout.addLayout(grid)
        layout.addWidget(self.cb_compress)
        
        btn_layout = QHBoxLayout()
        btn_layout.addWidget(self.btn_preview)
        btn_layout.addWidget(self.btn_export)
        layout.addLayout(btn_layout)

        self.setLayout(layout)

    def select_file(self):
        file_path, _ = QFileDialog.getOpenFileName(
            self, "选择音视频文件", "", 
            "Audio/Video Files (*.mp4 *.mp3 *.wav *.ogg *.m4a *.flac *.mkv *.avi)"
        )
        if file_path:
            self.lbl_file.setText(file_path)

    def update_volume_label(self, value):
        self.lbl_volume.setText(f'音量调整 ({value}%):')

    def toggle_compression_box(self, fmt):
        # 当选择 wav 或 ogg 或 flac 等格式时，允许选择是否压缩
        if fmt in ['wav', 'ogg', 'flac']:
            self.cb_compress.setEnabled(True)
        else:
            self.cb_compress.setEnabled(False)

    def build_ffmpeg_cmd(self, output_path, is_preview=False):
        """核心：根据用户输入构建 FFmpeg 命令"""
        input_file = self.lbl_file.text()
        if input_file == '未选择文件' or not os.path.exists(input_file):
            return None

        start_time = self.txt_start.text().strip()
        end_time = self.txt_end.text().strip()
        volume_scale = self.slider_volume.value() / 100.0
        out_fmt = self.combo_format.currentText()

        # 基础命令
        cmd = ['ffmpeg', '-y'] # -y 表示覆盖输出文件
        
        # 1. 截取时间 (把 -ss 放在 -i 前面可以加快速度)
        if start_time:
            cmd.extend(['-ss', start_time])
        if end_time and not is_preview: # 试听时我们限制时长，不采用用户结束时间
            cmd.extend(['-to', end_time])
        elif is_preview:
            cmd.extend(['-t', '10']) # 试听默认只截取10秒，避免生成大文件

        cmd.extend(['-i', input_file])

        # 2. 音量调整
        # ffmpeg 音量滤镜：-filter:a "volume=1.5"
        cmd.extend(['-filter:a', f'volume={volume_scale}'])

        # 3. 格式与压缩处理
        if is_preview:
            # 试听统一转成低码率 mp3 方便快速播放
            cmd.extend(['-f', 'mp3', '-b:a', '128k'])
        else:
            if out_fmt == 'mp3':
                cmd.extend(['-b:a', '192k']) # 默认高清mp3码率
            elif out_fmt == 'wav':
                if self.cb_compress.isChecked():
                    # wav 压缩通常指转为 16位 ADPCM 或比 32位 float 更小的格式
                    cmd.extend(['-acodec', 'pcm_s16le']) 
                else:
                    cmd.extend(['-acodec', 'pcm_f32le'])
            elif out_fmt == 'ogg':
                if self.cb_compress.isChecked():
                    cmd.extend(['-q:a', '2']) # 低音质降低体积
                else:
                    cmd.extend(['-q:a', '6']) # 高音质
            elif out_fmt == 'm4a':
                cmd.extend(['-c:a', 'aac', '-b:a', '160k'])

        cmd.append(output_path)
        return cmd

    def preview_audio(self):
        """试听功能"""
        if self.lbl_file.text() == '未选择文件':
            QMessageBox.warning(self, '错误', '请先选择一个文件！')
            return
        
        self.player.stop() # 停止之前的播放
        
        cmd = self.build_ffmpeg_cmd(self.temp_preview_path, is_preview=True)
        
        try:
            # 隐藏黑窗口执行 ffmpeg
            startupinfo = None
            if os.name == 'nt':
                startupinfo = subprocess.STARTUPINFO()
                startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
                
            subprocess.run(cmd, startupinfo=startupinfo, check=True)
            
            # 播放生成的临时预览音频
            self.player.setSource(QUrl.fromLocalFile(os.path.abspath(self.temp_preview_path)))
            self.audio_output.setVolume(1.0) # 这里的音量是系统播放音量，保持100%
            self.player.play()
            QMessageBox.information(self, '试听', '正在试听截取的前10秒片段...')
            
        except Exception as e:
            QMessageBox.critical(self, '错误', f'生成试听失败: {str(e)}')

    def export_audio(self):
        """导出功能"""
        input_file = self.lbl_file.text()
        if input_file == '未选择文件':
            QMessageBox.warning(self, '错误', '请先选择一个文件！')
            return

        out_fmt = self.combo_format.currentText()
        # 弹出保存文件对话框
        save_path, _ = QFileDialog.getSaveFileName(
            self, "保存导出文件", f"output.{out_fmt}", f"Audio Files (*.{out_fmt})"
        )
        
        if not save_path:
            return

        cmd = self.build_ffmpeg_cmd(save_path, is_preview=False)
        
        try:
            self.btn_export.setText('导出中...')
            self.btn_export.setEnabled(False)
            QApplication.processEvents() # 刷新界面避免假死

            startupinfo = None
            if os.name == 'nt':
                startupinfo = subprocess.STARTUPINFO()
                startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW

            subprocess.run(cmd, startupinfo=startupinfo, check=True)
            QMessageBox.information(self, '成功', f'文件已成功导出至:\n{save_path}')
            
        except Exception as e:
            QMessageBox.critical(self, '错误', f'导出失败: {str(e)}')
        finally:
            self.btn_export.setText('开始导出')
            self.btn_export.setEnabled(True)

    def closeEvent(self, event):
        """关闭窗口时清理临时文件"""
        self.player.stop()
        if os.path.exists(self.temp_preview_path):
            try:
                os.remove(self.temp_preview_path)
            except:
                pass
        event.accept()

if __name__ == '__main__':
    app = QApplication(sys.argv)
    ex = AudioConverterApp()
    ex.show()
    sys.exit(app.exec())