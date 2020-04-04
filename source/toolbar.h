//
//  Toolbar - My Dear ImGUI toolbar window
//  Right now, my thinking is you dock this, maybe I can force it to be docked
//
#ifndef TOOLBAR_H_
#define TOOLBAR_H_

class Toolbar
{
public:
	Toolbar();
	~Toolbar();

	void Render();

private:

	unsigned int m_GLImage;
	float m_UV[4];

};

#endif

