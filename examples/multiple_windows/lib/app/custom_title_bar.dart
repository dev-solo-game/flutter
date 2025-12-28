// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ignore_for_file: invalid_use_of_internal_member
// ignore_for_file: implementation_imports

import 'package:flutter/material.dart';
import 'package:flutter/src/widgets/_window.dart';

/// 自定义标题栏组件
///
/// 提供窗口拖拽、最小化、最大化/还原、关闭功能
class CustomTitleBar extends StatelessWidget {
  const CustomTitleBar({
    super.key,
    required this.title,
    this.onClose,
    this.onMinimize,
    this.onMaximize,
    this.onDragStart,
    this.onDragUpdate,
    this.onDragEnd,
    this.showMinimize = true,
    this.showMaximize = true,
    this.showClose = true,
    this.isMaximized = false,
    this.backgroundColor,
    this.foregroundColor,
    this.height = 32.0,
  });

  /// 窗口标题
  final String title;

  /// 关闭按钮回调
  final VoidCallback? onClose;

  /// 最小化按钮回调
  final VoidCallback? onMinimize;

  /// 最大化/还原按钮回调
  final VoidCallback? onMaximize;

  /// 拖拽开始回调
  final Function(DragStartDetails)? onDragStart;

  /// 拖拽更新回调
  final Function(DragUpdateDetails)? onDragUpdate;

  /// 拖拽结束回调
  final Function(DragEndDetails)? onDragEnd;

  /// 是否显示最小化按钮
  final bool showMinimize;

  /// 是否显示最大化按钮
  final bool showMaximize;

  /// 是否显示关闭按钮
  final bool showClose;

  /// 当前是否处于最大化状态
  final bool isMaximized;

  /// 标题栏背景颜色
  final Color? backgroundColor;

  /// 标题栏前景颜色（文字和图标）
  final Color? foregroundColor;

  /// 标题栏高度
  final double height;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final bgColor = backgroundColor ?? theme.colorScheme.surface;
    final fgColor = foregroundColor ?? theme.colorScheme.onSurface;

    return Container(
      height: height,
      decoration: BoxDecoration(
        color: bgColor,
        border: Border(
          bottom: BorderSide(
            color: theme.dividerColor,
            width: 1.0,
          ),
        ),
      ),
      child: Row(
        children: [
          // 窗口图标和标题（可拖拽区域）
          Expanded(
            child: GestureDetector(
              behavior: HitTestBehavior.translucent,
              onPanStart: onDragStart,
              onPanUpdate: onDragUpdate,
              onPanEnd: onDragEnd,
              child: Row(
                children: [
                  const SizedBox(width: 12),
                  Icon(
                    Icons.window,
                    size: 16,
                    color: fgColor.withOpacity(0.7),
                  ),
                  const SizedBox(width: 8),
                  Expanded(
                    child: Text(
                      title,
                      style: TextStyle(
                        color: fgColor,
                        fontSize: 12,
                        fontWeight: FontWeight.w500,
                      ),
                      overflow: TextOverflow.ellipsis,
                    ),
                  ),
                ],
              ),
            ),
          ),
          // 窗口控制按钮
          Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              if (showMinimize)
                _TitleBarButton(
                  icon: Icons.remove,
                  onPressed: onMinimize,
                  tooltip: '最小化',
                  foregroundColor: fgColor,
                ),
              if (showMaximize)
                _TitleBarButton(
                  icon: isMaximized ? Icons.filter_none : Icons.crop_square,
                  onPressed: onMaximize,
                  tooltip: isMaximized ? '还原' : '最大化',
                  foregroundColor: fgColor,
                ),
              if (showClose)
                _TitleBarButton(
                  icon: Icons.close,
                  onPressed: onClose,
                  tooltip: '关闭',
                  foregroundColor: fgColor,
                  hoverColor: Colors.red,
                  isCloseButton: true,
                ),
            ],
          ),
        ],
      ),
    );
  }
}

/// 标题栏按钮
class _TitleBarButton extends StatefulWidget {
  const _TitleBarButton({
    required this.icon,
    required this.onPressed,
    required this.tooltip,
    required this.foregroundColor,
    this.hoverColor,
    this.isCloseButton = false,
  });

  final IconData icon;
  final VoidCallback? onPressed;
  final String tooltip;
  final Color foregroundColor;
  final Color? hoverColor;
  final bool isCloseButton;

  @override
  State<_TitleBarButton> createState() => _TitleBarButtonState();
}

class _TitleBarButtonState extends State<_TitleBarButton> {
  bool _isHovered = false;

  @override
  Widget build(BuildContext context) {
    return MouseRegion(
      onEnter: (_) => setState(() => _isHovered = true),
      onExit: (_) => setState(() => _isHovered = false),
      child: Tooltip(
        message: widget.tooltip,
        child: GestureDetector(
          onTap: widget.onPressed,
          child: Container(
            width: 46,
            height: 32,
            decoration: BoxDecoration(
              color: _isHovered
                  ? (widget.hoverColor ?? widget.foregroundColor.withOpacity(0.1))
                  : Colors.transparent,
            ),
            child: Icon(
              widget.icon,
              size: 16,
              color: _isHovered && widget.isCloseButton
                  ? Colors.white
                  : widget.foregroundColor,
            ),
          ),
        ),
      ),
    );
  }
}

/// Regular 窗口专用标题栏
///
/// 自动从 [RegularWindowController] 获取窗口状态
class RegularWindowTitleBar extends StatefulWidget {
  const RegularWindowTitleBar({
    super.key,
    required this.controller,
    this.backgroundColor,
    this.foregroundColor,
    this.height = 32.0,
  });

  final RegularWindowController controller;
  final Color? backgroundColor;
  final Color? foregroundColor;
  final double height;

  @override
  State<RegularWindowTitleBar> createState() => _RegularWindowTitleBarState();
}

class _RegularWindowTitleBarState extends State<RegularWindowTitleBar> {
  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: widget.controller,
      builder: (context, _) {
        return CustomTitleBar(
          title: widget.controller.title,
          isMaximized: widget.controller.isMaximized,
          backgroundColor: widget.backgroundColor,
          foregroundColor: widget.foregroundColor,
          height: widget.height,
          showMinimize: true,
          showMaximize: true,
          showClose: true,
          onMinimize: () => widget.controller.setMinimized(true),
          onMaximize: () => widget.controller.setMaximized(!widget.controller.isMaximized),
          onClose: () => widget.controller.destroy(),
          onDragStart: (details) {
            // 拖拽开始 - 状态 0
            widget.controller.dragWindow(0);
          },
          onDragUpdate: (details) {
            // 拖拽更新 - 状态 1
            widget.controller.dragWindow(1);
          },
          onDragEnd: (details) {
            // 拖拽结束 - 状态 2
            widget.controller.dragWindow(2);
          },
        );
      },
    );
  }
}

/// Dialog 窗口专用标题栏
///
/// 自动从 [DialogWindowController] 获取窗口状态
class DialogWindowTitleBar extends StatefulWidget {
  const DialogWindowTitleBar({
    super.key,
    required this.controller,
    this.backgroundColor,
    this.foregroundColor,
    this.height = 32.0,
  });

  final DialogWindowController controller;
  final Color? backgroundColor;
  final Color? foregroundColor;
  final double height;

  @override
  State<DialogWindowTitleBar> createState() => _DialogWindowTitleBarState();
}

class _DialogWindowTitleBarState extends State<DialogWindowTitleBar> {
  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: widget.controller,
      builder: (context, _) {
        return CustomTitleBar(
          title: widget.controller.title,
          isMaximized: false,
          backgroundColor: widget.backgroundColor,
          foregroundColor: widget.foregroundColor,
          height: widget.height,
          showMinimize: widget.controller.parent == null, // 模态对话框不显示最小化
          showMaximize: false, // 对话框不支持最大化
          showClose: true,
          onMinimize: widget.controller.parent == null
              ? () => widget.controller.setMinimized(true)
              : null,
          onClose: () => widget.controller.destroy(),
          onDragStart: (details) {
            // 拖拽开始 - 状态 0
            widget.controller.dragWindow(0);
          },
          onDragUpdate: (details) {
            // 拖拽更新 - 状态 1
            widget.controller.dragWindow(1);
          },
          onDragEnd: (details) {
            // 拖拽结束 - 状态 2
            widget.controller.dragWindow(2);
          },
        );
      },
    );
  }
}
