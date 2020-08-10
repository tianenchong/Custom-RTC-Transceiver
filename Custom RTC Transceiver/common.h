#pragma once

enum VP8PassMode	//self explanatory
{
	kPassModeOnePass = 0,
	kPassModeFirstPass = (kPassModeOnePass + 1),
	kPassModeLastPass = (kPassModeFirstPass + 1)
};