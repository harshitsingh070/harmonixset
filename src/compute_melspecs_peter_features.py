#!/usr/bin/env python
# -*-coding:utf-8 -*-
"""
@File    :   compute_melspecs_peter_features.py
@Time    :   2024/12/06 11:51:01
@Author  :   Oriol Nieto
@Contact :   onieto@adobe.com
@License :   (C)Copyright 2024, Adobe Research
@Desc    :   <TODO>
"""

import os

import librosa
import numpy as np
import pandas as pd
import torch
import torchaudio
from tqdm import tqdm

# input
source_mp3_path = "../dataset/audio"
metadata_file = "../dataset/metadata.csv"

# output
specs_path = "./out_peter"

# device
device = "cpu"  # 'cuda' or 'cpu'

SAMPLE_RATE = 44100

# ensure path is present
os.makedirs(specs_path, exist_ok=True)


class LogMelSpectrogram(torch.nn.Module):
    def __init__(
        self,
        sample_rate=44100,
        n_fft=1024,
        hop_length=441,
        f_min=30,
        f_max=11000,
        n_mels=128,
        mel_scale="slaney",
        normalized="frame_length",
        power=1,
        log_multiplier=1000,
        device="cpu",
    ):
        super().__init__()
        self.spect_class = torchaudio.transforms.MelSpectrogram(
            sample_rate=sample_rate,
            n_fft=n_fft,
            hop_length=hop_length,
            f_min=f_min,
            f_max=f_max,
            n_mels=n_mels,
            mel_scale=mel_scale,
            normalized=normalized,
            power=power,
        ).to(device)
        self.log_multiplier = log_multiplier

    def forward(self, x):
        return torch.log1p(self.log_multiplier * self.spect_class(x).T)


preprocessor = LogMelSpectrogram().to(device)


def generate_spectograms(id, audio_path):
    spectogram_path_full = f"{specs_path}/{id}_full.npy"
    if os.path.exists(spectogram_path_full):
        return True

    y, sr = librosa.load(audio_path, sr=44100, mono=True)
    waveform = torch.FloatTensor(y).to(device)
    spec = preprocessor(waveform)

    np.save(spectogram_path_full, spec)

    return True


metadata = pd.read_csv(metadata_file)
total_items = len(metadata)
for index, row in tqdm(metadata.iterrows(), total=total_items, desc="Processing"):
    id = row["File"]
    audio_path = f"{source_mp3_path}/{id}.mp3"
    generate_spectograms(id, audio_path)
