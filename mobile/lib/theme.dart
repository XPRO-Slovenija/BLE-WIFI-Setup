import 'package:flutter/material.dart';

const _seed = Color(0xFFC51A4A); // Raspberry Pi raspberry

ThemeData lightTheme = ThemeData(
  colorScheme: ColorScheme.fromSeed(seedColor: _seed),
  useMaterial3: true,
);

ThemeData darkTheme = ThemeData(
  colorScheme: ColorScheme.fromSeed(seedColor: _seed, brightness: Brightness.dark),
  useMaterial3: true,
);
