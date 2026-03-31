// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';
import 'dart:ui' as ui;

import 'package:android_driver_extensions/extension.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_driver/driver_extension.dart';

import '../src/allow_list_devices.dart';

Future<int> fetchTexture(String channel, int width, int height) async {
  final int? result = await MethodChannel(channel).invokeMethod<int>(
    'initTexture',
    <String, int>{'width': width, 'height': height},
  );
  return result!;
}

void createImageFromTextureMain(String channel) async {
  ensureAndroidDevice();
  enableFlutterDriverExtension(commands: <CommandExtension>[nativeDriverCommands]);

  SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersive);

  final Future<int> textureId = fetchTexture(channel, 512, 512);
  runApp(CreateImageFromTextureApp(textureId));
}

final class CreateImageFromTextureApp extends StatelessWidget {
  const CreateImageFromTextureApp(this.textureId, {super.key});
  final Future<int> textureId;

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      home: FutureBuilder<int>(
        future: textureId,
        builder: (BuildContext context, AsyncSnapshot<int> snapshot) {
          if (snapshot.hasData) {
            return RawImage(image: ui.createImageFromTexture(snapshot.data!, width: 512, height: 512));
          }
          return const CircularProgressIndicator();
        },
      ),
    );
  }
}
