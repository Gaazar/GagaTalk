#pragma once

class http_client
{

public:
	virtual void on_data(char* data, int size);
	virtual void on_finish();
};