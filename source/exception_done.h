/*
	EXCEPTION_DONE.H
	----------------
	Copyright (c) 2023 Vaughan Kitchen
	Released under the 2-clause BSD license (See:https://en.wikipedia.org/wiki/BSD_licenses)
*/
/*!
	@file
	@brief Exception to indicate processing has finished and should be early aborted at a greater depth than return handles
	@author Vaughan Kitchen
	@copyright 2023 Vaughan Kitchen
*/
#pragma once

class Done : public std::exception
	{
	public:
		virtual const char *what() const noexcept
			{
			return "Early return";
			}
	};
