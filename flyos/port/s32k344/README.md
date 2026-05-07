# S32K344 Port Notes

This directory contains the S32K344 public trial port for `flyos-standard`.

## Intended use

Use this port when evaluating the public kernel line on an S32K344-based Cortex-M7 target.

## Expected surrounding environment

Your board project should already provide:

- startup code;
- vector table;
- clock initialization;
- NXP SDK integration;
- linker-script support.

## Public trial goal

The goal of this port in the first public release is to support kernel trial, integration understanding, and feedback collection.
