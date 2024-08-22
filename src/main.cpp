#include <Geode/Geode.hpp>
#include <set>
#include <fstream>
#include <unordered_map>

#include "json.hpp"
#include "emojis.hpp"

#define EMOJI_RECT_SIZE 44

using namespace geode::prelude;
using namespace nlohmann;

static bool emojiInitialized = false;

struct Emoji {
	std::string name;
	std::string category;
	int x, y;
};

static std::vector<Emoji> emojis;

static void initEmojis() {
	if (emojiInitialized) return;

	std::ifstream file("emojis.json"_spr);
	json emojis_json = json::parse(emojis_json_source);

	std::vector<json> array = emojis_json.get<std::vector<json>>();

	for (auto emoji : array) {
		Emoji e;
		e.name = emoji["name"].get<std::string>();
		e.category = emoji["category"].get<std::string>();
		std::vector<int> rect = emoji["pos"].get<std::vector<int>>();
		e.x = rect[0];
		e.y = rect[1];

		emojis.push_back(e);
	}
	log::info("Loaded {} emojis", array.size());

	file.close();

	emojiInitialized = true;
}

static bool isEmojiCharName(int c) {
	return c == '_' || isalnum(c);
}

static Emoji* findNextEmoji(unsigned short* str, int index, int& next_index) {
	if (str[index] != ':') return nullptr;
	int start = index + 1;
	int end = start;
	while (str[end]) {
		if (!isEmojiCharName(str[end])) break;
		end++;
	}
	if (str[end] != ':') return nullptr;
	std::string name(end - start, ' ');
	for (int i = 0; i < end - start; i++)
		name[i] = str[start + i];
	for (auto& emoji : emojis) {
		if (emoji.name == name) {
			next_index = end + 1;
			return &emoji;
		}
	}
	return nullptr;
}

static int emojiSize = 32;

static std::vector<CCLabelBMFont*> emojiLabels;

#include <Geode/modify/CCLabelBMFont.hpp>
class $modify(CCLabelBMFont) {
	bool initWithString(const char *str, const char *fntFile, float width, CCTextAlignment alignment, CCPoint imageOffset) {
		std::string tmp;
		if (strcmp(fntFile, "chatFont.fnt") == 0) {
			switch (GameManager::get()->m_texQuality) {
			case 1: tmp = "chatFont.fnt"_spr; emojiSize = 16; break;
			case 2: tmp = "chatFont_hd.fnt"_spr; emojiSize = 32; break;
			case 0:
			default:
			case 3: tmp = "chatFont_uhd.fnt"_spr; emojiSize = 64; break;
			}
			fntFile = tmp.c_str();
			emojiLabels.push_back(this);
		}
		return CCLabelBMFont::initWithString(str, fntFile, width, alignment, imageOffset);
	}

	// ILIAS: Most of this code is copied from CCLabelBMFont::createFontChars
	void createFontChars() {
		// MOD START: Initializing emojis
		bool isEmojiLabel = std::find(emojiLabels.begin(), emojiLabels.end(), this) != emojiLabels.end();
		if (isEmojiLabel)
			initEmojis();
		// MOD END

		int nextFontPositionX = 0;
		int nextFontPositionY = 0;
		unsigned short prev = -1;
		int kerningAmount = 0;

		CCSize tmpSize = CCSizeZero;

		int longestLine = 0;
		unsigned int totalHeight = 0;

		unsigned int quantityOfLines = 1;
		unsigned int stringLen = m_sString ? cc_wcslen(m_sString) : 0;
		if (stringLen == 0)
		{
			this->setContentSize(CC_SIZE_PIXELS_TO_POINTS(tmpSize));
			return;
		}

		std::set<unsigned int> *charSet = m_pConfiguration->getCharacterSet();

		for (unsigned int i = 0; i < stringLen - 1; ++i)
		{
			unsigned short c = m_sString[i];
			if (c == '\n')
			{
				quantityOfLines++;
			}
		}

		totalHeight = m_pConfiguration->m_nCommonHeight * quantityOfLines;
		nextFontPositionY = 0-(m_pConfiguration->m_nCommonHeight - m_pConfiguration->m_nCommonHeight * quantityOfLines);
		
		CCRect rect;
		ccBMFontDef fontDef;
		// MOD START: Special font def for emojis
		ccBMFontDef emojiFontDef;
		emojiFontDef.charID = 0;
		emojiFontDef.xOffset = emojiSize/2 + 3;
		emojiFontDef.yOffset = 0;
		emojiFontDef.xAdvance = emojiSize + 4;
		// MOD END

		for (unsigned int i= 0; i < stringLen; i++)
		{
			unsigned short c = m_sString[i];

			// MOD START: Checking if there is an emoji at this index
			int next_index;
			Emoji* emoji = nullptr;
			if (isEmojiLabel)
				emoji = findNextEmoji(m_sString, i, next_index);
			// MOD END

			if (c == '\n')
			{
				nextFontPositionX = 0;
				nextFontPositionY -= m_pConfiguration->m_nCommonHeight;
				continue;
			}
			
			if (charSet->find(c) == charSet->end())
			{
				CCLOGWARN("cocos2d::CCLabelBMFont: Attempted to use character not defined in this bitmap: %d", c);
				continue;      
			}

			{ // ILIAS: kerningAmoundForFirst(first, second) is private so i had to copy it here
				unsigned short first = prev;
				unsigned short second = c;
				int ret = 0;
				unsigned int key = (first<<16) | (second & 0xffff);

				if( m_pConfiguration->m_pKerningDictionary ) {
					tCCKerningHashElement *element = NULL;
					HASH_FIND_INT(m_pConfiguration->m_pKerningDictionary, &key, element);        
					if(element)
						ret = element->amount;
				}
				kerningAmount = ret;
			}
			
			tCCFontDefHashElement *element = NULL;

			// unichar is a short, and an int is needed on HASH_FIND_INT
			unsigned int key = c;
			HASH_FIND_INT(m_pConfiguration->m_pFontDefDictionary, &key, element);
			if (! element)
			{
				CCLOGWARN("cocos2d::CCLabelBMFont: characer not found %d", c);
				continue;
			}

			fontDef = element->fontDef;

			// MOD START: Use a special font def for emojis
			if (emoji)
				fontDef = emojiFontDef;
			// MOD END

			rect = fontDef.rect;
			rect = CC_RECT_PIXELS_TO_POINTS(rect);

			rect.origin.x += m_tImageOffset.x;
			rect.origin.y += m_tImageOffset.y;

			if (emoji)
				rect = CC_RECT_PIXELS_TO_POINTS(CCRectMake(emoji->x * emojiSize, (emoji->y + 8) * emojiSize, emojiSize, emojiSize));

			CCSprite *fontChar;

			bool hasSprite = true;

			fontChar = (CCSprite*)(this->getChildByTag(i));

			if(fontChar )
			{
				// Reusing previous Sprite
				fontChar->setVisible(true);
			}
			else
			{
				// New Sprite ? Set correct color, opacity, etc...
				if( 0 )
				{
					/* WIP: Doesn't support many features yet.
					But this code is super fast. It doesn't create any sprite.
					Ideal for big labels.
					*/
					fontChar = m_pReusedChar;
					fontChar->setBatchNode(NULL);
					hasSprite = false;
				}
				else
				{
					fontChar = new CCSprite();
					fontChar->initWithTexture(m_pobTextureAtlas->getTexture(), rect);
					addChild(fontChar, i, i);
					fontChar->release();
				}
				
				// Apply label properties
				fontChar->setOpacityModifyRGB(m_bIsOpacityModifyRGB);
				
				// Color MUST be set before opacity, since opacity might change color if OpacityModifyRGB is on
				fontChar->updateDisplayedColor(m_tDisplayedColor);
				fontChar->updateDisplayedOpacity(m_cDisplayedOpacity);
			}

			// updating previous sprite
			fontChar->setTextureRect(rect, false, rect.size);

			// See issue 1343. cast( signed short + unsigned integer ) == unsigned integer (sign is lost!)
			int yOffset = m_pConfiguration->m_nCommonHeight - fontDef.yOffset;
			CCPoint fontPos = ccp( (float)nextFontPositionX + fontDef.xOffset + fontDef.rect.size.width*0.5f + kerningAmount,
				(float)nextFontPositionY + yOffset - rect.size.height*0.5f * CC_CONTENT_SCALE_FACTOR() );
			fontChar->setPosition(CC_POINT_PIXELS_TO_POINTS(fontPos));

			// update kerning
			nextFontPositionX += fontDef.xAdvance + kerningAmount;
			prev = c;

			if (longestLine < nextFontPositionX)
			{
				longestLine = nextFontPositionX;
			}
			
			if (! hasSprite)
			{
				updateQuadFromSprite(fontChar, i);
			}

			// MOD START: Skipping all characters that are part of the emoji text
			if (emoji)
				i = next_index - 1;
			// MOD END
		}

		// If the last character processed has an xAdvance which is less that the width of the characters image, then we need
		// to adjust the width of the string to take this into account, or the character will overlap the end of the bounding
		// box
		if (fontDef.xAdvance < fontDef.rect.size.width)
		{
			tmpSize.width = longestLine + fontDef.rect.size.width - fontDef.xAdvance;
		}
		else
		{
			tmpSize.width = longestLine;
		}
		tmpSize.height = totalHeight;

		this->setContentSize(CC_SIZE_PIXELS_TO_POINTS(tmpSize));
	}
};